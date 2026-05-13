#include "EPTio.h"
#include "Header.h"
#include "LASio.h"
#include "error.h"
#include "print.h"

#include <nlohmann/json.hpp>

#ifdef USING_GDAL
#include <cpl_vsi.h>
#endif

#include <fstream>
#include <future>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <stdexcept>
#include <cmath>
#include <utility>
#include <vector>

// Detect remote schemes regardless of GDAL availability so callers can produce
// a clear "requires GDAL" error rather than letting an HTTPS URL fall through
// to ifstream as if it were a local path.
static bool is_remote(const std::string& path)
{
  if (path.compare(0, 7, "http://") == 0) return true;
  if (path.compare(0, 8, "https://") == 0) return true;
  if (path.compare(0, 9, "/vsicurl/") == 0) return true;
  if (path.compare(0, 7, "/vsis3/") == 0) return true;
  if (path.compare(0, 7, "/vsigs/") == 0) return true;
  if (path.compare(0, 7, "/vsiaz/") == 0) return true;
  if (path.compare(0, 9, "/vsiadls/") == 0) return true;
  if (path.compare(0, 8, "/vsioss/") == 0) return true;
  if (path.compare(0, 10, "/vsiswift/") == 0) return true;
  return false;
}

EPTio::EPTio()
{
  remote = false;
  opened = false;
  srs_epsg = 0;
  depth_limit = -1;
  total_points = 0;
  current_tile = nullptr;
  points_read = 0;

  // Prefetch depth. Default 4 saturates AWS S3 from a single EPTio (matches
  // PDAL's readers.ept requests=15 default, capped at 4 since that's the
  // measured plateau). Env override LASR_EPT_PREFETCH clamps to [1, 32].
  prefetch_depth = 4;
  if (const char* env = std::getenv("LASR_EPT_PREFETCH"))
  {
    try {
      int v = std::stoi(env);
      if (v >= 1 && v <= 32) prefetch_depth = v;
    } catch (...) { /* keep default on garbage */ }
  }

  for (int i = 0; i < 6; i++)
  {
    cube_bounds[i] = 0;
    conf_bounds[i] = 0;
  }
}

EPTio::~EPTio()
{
  close();
}

void EPTio::open(const std::string& endpoint)
{
  if (opened)
    throw std::logic_error("Internal error: EPTio already opened");

  // Short-circuit: use metadata from index if available
  if (index) {
    this->remote = index->remote;
    this->base_path = index->base_path;
    this->query_string = index->query_string;
    this->ept_metadata = index->ept_metadata;
    for (int i = 0; i < 6; ++i) {
      cube_bounds[i] = index->cube_bounds[i];
      conf_bounds[i] = index->conf_bounds[i];
    }
    this->srs_wkt = index->srs_wkt;
    this->srs_epsg = index->srs_epsg;
    this->probe_tile_path = index->probe_tile_path;
    opened = true;
    return;
  }

  // Determine if remote
  this->remote = is_remote(endpoint);

  // Separate query string from path (for signed URLs like ?token=...)
  std::string clean_endpoint = endpoint;
  size_t qpos = endpoint.find('?');
  if (qpos != std::string::npos)
  {
    query_string = endpoint.substr(qpos);
    clean_endpoint = endpoint.substr(0, qpos);
  }

  // Derive base_path by stripping ept.json
  base_path = clean_endpoint;
  size_t pos = base_path.rfind("ept.json");
  if (pos != std::string::npos)
    base_path = base_path.substr(0, pos);
  else
    throw std::runtime_error("EPT endpoint must point to an ept.json file: " + endpoint);

  // Ensure trailing slash
  if (!base_path.empty() && base_path.back() != '/')
    base_path += '/';

  parse_ept_json();
  opened = true;
  find_probe_tile();
}

void EPTio::find_probe_tile()
{
  // Walk the hierarchy (BFS) until we find a real tile we can open, and record
  // its path for later use by populate_header(). Descend into sub-hierarchy
  // pages (point_count == -1) as needed so sparse/deep hierarchies still yield
  // a usable result. Fail loudly rather than leave probe_tile_path empty.
  std::deque<EPTkey> pages_to_visit;
  pages_to_visit.push_back(EPTkey(0, 0, 0, 0));

  while (!pages_to_visit.empty())
  {
    EPTkey page_key = pages_to_visit.front();
    pages_to_visit.pop_front();

    std::string hier_path = hierarchy_path(page_key);
    std::string json_str;
    try { json_str = read_file_contents(hier_path); }
    catch (...) { continue; }

    nlohmann::json hierarchy = nlohmann::json::parse(json_str);

    // First pass: try to open any leaf tile in this page
    for (auto& [key_str, value] : hierarchy.items())
    {
      int point_count = value.get<int>();
      if (point_count <= 0) continue;

      int d, x, y, z;
      if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4)
        continue;

      EPTkey key(d, x, y, z);
      std::string path = tile_path(key);

      try
      {
        LASio probe;
        probe.open(path);
        probe.close();
        probe_tile_path = path;
        return;
      }
      catch (...) { continue; }
    }

    // Second pass: queue any sub-hierarchy pages for further traversal
    for (auto& [key_str, value] : hierarchy.items())
    {
      if (value.get<int>() != -1) continue;
      int d, x, y, z;
      if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4)
        continue;
      pages_to_visit.push_back(EPTkey(d, x, y, z));
    }
  }

  throw std::runtime_error("EPT dataset has no readable tiles to derive schema from: " + base_path);
}

void EPTio::parse_ept_json()
{
  // query_string is appended for signed URL authentication
  std::string json_str = read_file_contents(base_path + "ept.json" + query_string);
  ept_metadata = nlohmann::json::parse(json_str);

  // Validate dataType
  std::string data_type = ept_metadata.value("dataType", "");
  if (data_type != "laszip")
    throw std::runtime_error("EPT dataset uses '" + data_type + "' format, only 'laszip' is supported");

  // Read bounds (octree cube bounds)
  auto bounds = ept_metadata["bounds"];
  if (!bounds.is_array() || bounds.size() != 6)
    throw std::runtime_error("Invalid EPT bounds in ept.json");

  for (int i = 0; i < 6; i++)
    cube_bounds[i] = bounds[i].get<double>();

  // Read conforming bounds if available, otherwise use cube bounds
  if (ept_metadata.contains("boundsConforming"))
  {
    auto cbounds = ept_metadata["boundsConforming"];
    if (cbounds.is_array() && cbounds.size() == 6)
    {
      for (int i = 0; i < 6; i++)
        conf_bounds[i] = cbounds[i].get<double>();
    }
    else
    {
      for (int i = 0; i < 6; i++)
        conf_bounds[i] = cube_bounds[i];
    }
  }
  else
  {
    for (int i = 0; i < 6; i++)
      conf_bounds[i] = cube_bounds[i];
  }

  // Read SRS
  if (ept_metadata.contains("srs"))
  {
    auto srs = ept_metadata["srs"];
    srs_wkt = srs.value("wkt", "");
    std::string authority = srs.value("authority", "");
    std::string horizontal = srs.value("horizontal", "");
    if (srs_wkt.empty() && authority == "EPSG" && !horizontal.empty())
    {
      try { srs_epsg = std::stoi(horizontal); }
      catch (...) { srs_epsg = 0; }
    }
  }

  // Note: schema (attribute names, PDF, extra bytes, bit flags) is derived
  // from a real tile via LASio in populate_header(). Parsing the JSON schema
  // directly would miss naming variations, extra attributes, and LAS bit flags
  // that aren't exposed in ept.json.
}

void EPTio::populate_header(Header* header, bool)
{
  if (!opened)
    throw std::logic_error("Internal error: EPTio not opened");

  if (probe_tile_path.empty())
    throw std::logic_error("Internal error: no probe tile available to derive EPT schema");

  // Delegate schema construction (PDF, attributes, extra bytes, bit flags,
  // scale/offset) to LASio on a real tile. This avoids fragile guesses from
  // the EPT JSON schema, which uses inconsistent naming conventions across
  // datasets ("GpsTime"/"gpstime", "Red"/"red", "Infrared"/"NIR") and omits
  // LAS bit flags entirely.
  LASio probe;
  probe.open(probe_tile_path);
  probe.populate_header(header);
  probe.close();

  // Override EPT-specific fields. The schema and scale/offset already came
  // from the probe tile.
  header->signature = "EPTF";

  // Use conforming bounds from ept.json (authoritative for the dataset,
  // not just the probe tile)
  header->min_x = conf_bounds[0];
  header->min_y = conf_bounds[1];
  header->min_z = conf_bounds[2];
  header->max_x = conf_bounds[3];
  header->max_y = conf_bounds[4];
  header->max_z = conf_bounds[5];

  // CRS from ept.json (overrides whatever LASio picked up from the tile VLRs)
  if (!srs_wkt.empty())
    header->set_crs(srs_wkt);
  else if (srs_epsg > 0)
    header->set_crs(srs_epsg);

  // Spatial index is always true for EPT (octree-indexed)
  header->spatial_index = true;

  // Point count from hierarchy traversal (populated during query)
  header->number_of_point_records = total_points;
}

void EPTio::query(const std::vector<std::string>& main_files,
                  const std::vector<std::string>&,
                  double xmin, double ymin, double xmax, double ymax,
                  double buffer, bool,
                  std::vector<std::string> filters)
{
  if (main_files.empty())
    throw std::invalid_argument("EPT reader requires at least one file path");

  // Reset depth limit so a previous query's setting doesn't carry over.
  depth_limit = -1;

  // Parse -depth from filters (injected as "-depth N" by the API)
  for (const auto& filter : filters)
  {
    size_t start = filter.find_first_not_of(" \t");
    std::string trimmed = (start == std::string::npos) ? "" : filter.substr(start);
    if (trimmed.compare(0, 7, "-depth ") == 0)
    {
      try { depth_limit = std::stoi(trimmed.substr(7)); }
      catch (...) { depth_limit = -1; }
    }
  }

  // Open the EPT endpoint
  if (!opened)
    open(main_files[0]);

  // Drain any in-flight prefetch from a prior query so it can't write
  // into the queues we are about to reset.
  cancel_prefetch();

  // Clear previous state
  tile_queue.clear();
  total_points = 0;
  points_read = 0;

  if (current_tile)
  {
    current_tile->close();
    delete current_tile;
    current_tile = nullptr;
  }

  // Traverse hierarchy with spatial filter (expand by buffer)
  double qxmin = xmin - buffer;
  double qymin = ymin - buffer;
  double qxmax = xmax + buffer;
  double qymax = ymax + buffer;

  traverse_hierarchy(qxmin, qymin, qxmax, qymax);

  // Sort tiles spatially for efficient reading
  std::sort(tile_queue.begin(), tile_queue.end(), [this](const EPTkey& a, const EPTkey& b) {
    double ax = (double)a.x / (1 << a.d);
    double bx = (double)b.x / (1 << b.d);
    if (ax < bx) return true;
    if (ax > bx) return false;
    double ay = (double)a.y / (1 << a.d);
    double by = (double)b.y / (1 << b.d);
    if (ay < by) return true;
    if (ay > by) return false;
    return a.d < b.d;
  });
}

void EPTio::traverse_hierarchy(double qxmin, double qymin, double qxmax, double qymax)
{
  if (index && index->tiles_built) {
    for (const auto& t : index->tiles) {
      if (depth_limit >= 0 && t.key.d > depth_limit) continue;
      if (t.xmax < qxmin || t.xmin > qxmax || t.ymax < qymin || t.ymin > qymax) continue;
      tile_queue.push_back(t.key);
      total_points += t.point_count;
    }
    return;
  }
  EPTkey root(0, 0, 0, 0);
  load_hierarchy_page(root, qxmin, qymin, qxmax, qymax);
}

void EPTio::load_hierarchy_page(const EPTkey& page_key, double qxmin, double qymin, double qxmax, double qymax)
{
  std::string path = hierarchy_path(page_key);
  std::string json_str;
  bool is_root = (page_key.d == 0 && page_key.x == 0 && page_key.y == 0 && page_key.z == 0);

  try
  {
    json_str = read_file_contents(path);
  }
  catch (const std::exception& e)
  {
    if (is_root)
      throw std::runtime_error("Failed to read EPT hierarchy: " + path + ": " + e.what());

    warning("EPT sub-hierarchy file not found: %s\n", path.c_str());
    return;
  }

  nlohmann::json hierarchy = nlohmann::json::parse(json_str);

  for (auto& [key_str, value] : hierarchy.items())
  {
    // Parse key "D-X-Y-Z"
    int d, x, y, z;
    if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4)
      continue;

    EPTkey key(d, x, y, z);

    // Check depth limit
    if (depth_limit >= 0 && d > depth_limit)
      continue;

    // Compute node bounds and check intersection (2D)
    double nxmin, nymin, nzmin, nxmax, nymax, nzmax;
    compute_node_bounds(key, nxmin, nymin, nzmin, nxmax, nymax, nzmax);

    // 2D intersection test
    if (nxmax < qxmin || nxmin > qxmax || nymax < qymin || nymin > qymax)
      continue;

    int point_count = value.get<int>();

    if (point_count > 0)
    {
      // Node has points — add to queue
      tile_queue.push_back(key);
      total_points += point_count;
    }
    else if (point_count == -1)
    {
      // Sub-hierarchy exists — only recurse if deeper nodes are allowed
      if (depth_limit < 0 || d < depth_limit)
        load_hierarchy_page(key, qxmin, qymin, qxmax, qymax);
    }
    // point_count == 0: empty node, skip
  }
}

void EPTio::compute_node_bounds(const EPTkey& key,
                                double& nxmin, double& nymin, double& nzmin,
                                double& nxmax, double& nymax, double& nzmax) const
{
  double cube_size = cube_bounds[3] - cube_bounds[0]; // cube is isotropic
  double node_size = cube_size / (1 << key.d);

  nxmin = cube_bounds[0] + key.x * node_size;
  nymin = cube_bounds[1] + key.y * node_size;
  nzmin = cube_bounds[2] + key.z * node_size;
  nxmax = nxmin + node_size;
  nymax = nymin + node_size;
  nzmax = nzmin + node_size;
}

bool EPTio::read_point(Point* p)
{
  while (true)
  {
    // Try reading from current tile
    if (current_tile && current_tile->read_point(p))
    {
      points_read++;
      return true;
    }

    // Current tile exhausted, try next
    if (!open_next_tile())
      return false;
  }
}

EPTio::TileLoadResult EPTio::open_tile_sync(const EPTkey& key)
{
  TileLoadResult r;
  std::string path = tile_path(key);

#ifdef USING_GDAL
  // For remote tiles, bulk-download the .laz bytes here in the worker
  // thread and stash them in /vsimem/. The downstream LASio::open is then
  // a pure in-RAM operation, so the consumer's read_point() calls never
  // hit the network. This is the single biggest source of PDAL's per-thread
  // throughput advantage — its readers.ept does the same thing via libcurl
  // multi-handle.
  //
  // Without this, the prefetch worker only fetches the LAS header (~few KB)
  // and the bulk of the I/O still happens lazily in the consumer thread,
  // serialized with point decoding. Empirically that limited the speedup
  // from deepening the prefetch queue to <5%.
  if (remote)
  {
    std::string vsi_url =
      (path.compare(0, 4, "http") == 0) ? std::string("/vsicurl/") + path : path;

    VSILFILE* src = VSIFOpenL(vsi_url.c_str(), "rb");
    if (src == nullptr)
    {
      r.warning_msg = "Failed to open EPT tile (remote fetch) " + path;
      return r;
    }
    // Read the whole file into a vector — EPT tiles are bounded by the
    // span (typically a few MB compressed), so this is a small fixed cost.
    VSIFSeekL(src, 0, SEEK_END);
    vsi_l_offset sz = VSIFTellL(src);
    VSIFSeekL(src, 0, SEEK_SET);
    std::vector<unsigned char> buf;
    try { buf.resize(static_cast<size_t>(sz)); }
    catch (const std::bad_alloc&) {
      VSIFCloseL(src);
      r.warning_msg = "Out of memory buffering EPT tile " + path;
      return r;
    }
    size_t got = (sz > 0) ? VSIFReadL(buf.data(), 1, static_cast<size_t>(sz), src) : 0;
    VSIFCloseL(src);
    if (got != static_cast<size_t>(sz))
    {
      r.warning_msg = "Short read on EPT tile " + path +
                      " (expected " + std::to_string(sz) +
                      ", got " + std::to_string(got) + ")";
      return r;
    }

    // Unique /vsimem/ key. Mix tile key, EPTio instance pointer, and a
    // process-wide atomic counter so concurrent workers (across multiple
    // EPTio instances, multiple outer chunks) never collide.
    static std::atomic<uint64_t> vsimem_counter{0};
    uint64_t seq = vsimem_counter.fetch_add(1, std::memory_order_relaxed);
    char vsimem_buf[160];
    snprintf(vsimem_buf, sizeof(vsimem_buf),
             "/vsimem/lasR_ept/%p_%d-%d-%d-%d_%llu.laz",
             static_cast<const void*>(this), key.d, key.x, key.y, key.z,
             static_cast<unsigned long long>(seq));
    std::string vsimem_path = vsimem_buf;

    // VSIFileFromMemBuffer takes ownership iff bTakeOwnership=true. We pass
    // false and free the buffer ourselves at unlink time — except VSIFile
    // requires a heap buffer in either case. Allocate the canonical way
    // GDAL expects (CPLMalloc) and memcpy in. The cost vs std::move-style
    // ownership is one extra memcpy per tile (~few MB) — negligible next
    // to the network fetch we just saved.
    GByte* gdal_buf = static_cast<GByte*>(CPLMalloc(static_cast<size_t>(sz)));
    if (gdal_buf == nullptr)
    {
      r.warning_msg = "Out of memory staging EPT tile " + path + " in /vsimem/";
      return r;
    }
    std::memcpy(gdal_buf, buf.data(), static_cast<size_t>(sz));
    VSILFILE* mem = VSIFileFromMemBuffer(vsimem_path.c_str(), gdal_buf, sz, TRUE);
    if (mem == nullptr)
    {
      CPLFree(gdal_buf);
      r.warning_msg = "Failed to register /vsimem/ buffer for EPT tile " + path;
      return r;
    }
    VSIFCloseL(mem);

    LASio* tile = new LASio();
    try
    {
      tile->open(vsimem_path);
      Header temp_header;
      tile->populate_header(&temp_header);
      r.tile = tile;
      r.vsimem_path = vsimem_path;
      return r;
    }
    catch (const std::exception& e)
    {
      r.warning_msg = "Failed to open buffered EPT tile " + path + ": " + e.what();
      delete tile;
      VSIUnlink(vsimem_path.c_str());
      return r;
    }
  }
#endif

  // Local file path — open directly, no buffering needed.
  LASio* tile = new LASio();
  try
  {
    tile->open(path);
    Header temp_header;
    tile->populate_header(&temp_header);
    r.tile = tile;
    return r;
  }
  catch (const std::exception& e)
  {
    r.warning_msg = "Failed to open EPT tile " + path + ": " + e.what();
    delete tile;
    return r;
  }
}

void EPTio::prefetch_next_tile()
{
  // Top up the prefetch queue to prefetch_depth. std::async can throw under
  // heavy outer parallelism for two specified reasons: std::system_error
  // when the OS refuses to spawn another thread, and std::bad_alloc when
  // the shared state cannot be allocated. On failure we put the key back
  // and stop refilling so the consumer falls through to a synchronous
  // open. Catching std::exception covers both and any future failure mode.
  while ((int)prefetch_queue.size() < prefetch_depth && !tile_queue.empty())
  {
    EPTkey key = tile_queue.front();
    tile_queue.pop_front();
    try {
      prefetch_queue.push_back(std::async(std::launch::async,
        [this, key]() -> TileLoadResult { return open_tile_sync(key); }));
    }
    catch (const std::exception&) {
      tile_queue.push_front(key);
      break;  // OS is refusing threads; don't keep trying this turn
    }
  }
}

void EPTio::cancel_prefetch()
{
  // Drain every in-flight prefetch — cannot cancel std::async mid-flight,
  // so we just wait them out and free any LASio they produced. Warning
  // text on a leftover is harmless to drop; we already gave up on these.
  // /vsimem/ buffers held by leftovers must be unlinked or they leak.
  while (!prefetch_queue.empty())
  {
    TileLoadResult leftover = prefetch_queue.front().get();
    prefetch_queue.pop_front();
    if (leftover.tile)
    {
      leftover.tile->close();
      delete leftover.tile;
    }
#ifdef USING_GDAL
    if (!leftover.vsimem_path.empty())
      VSIUnlink(leftover.vsimem_path.c_str());
#endif
  }
}

bool EPTio::open_next_tile()
{
  // Close previous tile and release its /vsimem/ buffer (if any).
  if (current_tile)
  {
    current_tile->close();
    delete current_tile;
    current_tile = nullptr;
#ifdef USING_GDAL
    if (!current_vsimem_path.empty())
      VSIUnlink(current_vsimem_path.c_str());
#endif
    current_vsimem_path.clear();
  }

  // Keep the prefetch queue full before consuming. First call lazily
  // populates it; subsequent calls just top it back up to depth.
  prefetch_next_tile();

  // 1) Drain prefetched tiles in order, skipping any that failed to open.
  while (!prefetch_queue.empty())
  {
    TileLoadResult r = prefetch_queue.front().get();
    prefetch_queue.pop_front();
    if (!r.warning_msg.empty())
      warning("%s\n", r.warning_msg.c_str());   // emitted on consumer thread
    if (r.tile)
    {
      current_tile = r.tile;
      current_vsimem_path = r.vsimem_path;
      // Top up the queue so the next call already has bytes arriving.
      prefetch_next_tile();
      return true;
    }
    // Prefetch produced no tile but may still have left a /vsimem/ key
    // (e.g. LASio::open threw after we registered the buffer); clean it
    // up before moving on so we don't leak in-memory bytes.
#ifdef USING_GDAL
    if (!r.vsimem_path.empty())
      VSIUnlink(r.vsimem_path.c_str());
#endif
    // This prefetch failed — try the next one and keep the queue full.
    prefetch_next_tile();
  }

  // 2) Synchronous open fallback (only reached if every prefetch failed
  //    AND tile_queue is empty — i.e. the dataset is exhausted, or async
  //    dispatch keeps failing).
  if (tile_queue.empty()) return false;

  EPTkey key = tile_queue.front();
  tile_queue.pop_front();
  TileLoadResult r = open_tile_sync(key);
  if (!r.warning_msg.empty())
    warning("%s\n", r.warning_msg.c_str());
  if (!r.tile)
  {
#ifdef USING_GDAL
    if (!r.vsimem_path.empty())
      VSIUnlink(r.vsimem_path.c_str());
#endif
    return open_next_tile();  // try the next one
  }
  current_tile = r.tile;
  current_vsimem_path = r.vsimem_path;

  prefetch_next_tile();
  return true;
}

std::string EPTio::tile_path(const EPTkey& key) const
{
  return base_path + "ept-data/" +
    std::to_string(key.d) + "-" +
    std::to_string(key.x) + "-" +
    std::to_string(key.y) + "-" +
    std::to_string(key.z) + ".laz" + query_string;
}

std::string EPTio::hierarchy_path(const EPTkey& key) const
{
  return base_path + "ept-hierarchy/" +
    std::to_string(key.d) + "-" +
    std::to_string(key.x) + "-" +
    std::to_string(key.y) + "-" +
    std::to_string(key.z) + ".json" + query_string;
}

static std::string read_file_contents_impl(const std::string& path)
{
  if (is_remote(path))
  {
#ifdef USING_GDAL
    // Use GDAL VSI for remote files
    std::string vsi_path = path;
    if (path.compare(0, 4, "http") == 0)
      vsi_path = "/vsicurl/" + path;

    VSILFILE* fp = VSIFOpenL(vsi_path.c_str(), "rb");
    if (!fp)
      throw std::runtime_error("Cannot open remote file: " + path);

    // Read in chunks — seeking to end doesn't work reliably for HTTP streams
    std::string content;
    char buffer[8192];
    size_t bytes_read;
    while ((bytes_read = VSIFReadL(buffer, 1, sizeof(buffer), fp)) > 0)
      content.append(buffer, bytes_read);

    VSIFCloseL(fp);
    return content;
#else
    throw std::runtime_error("Remote EPT endpoints require GDAL support: " + path);
#endif
  }


  // Local file
  std::ifstream file(path);
  if (!file.is_open())
    throw std::runtime_error("Cannot open file: " + path);

  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

std::string EPTio::read_file_contents(const std::string& path) const
{
  return read_file_contents_impl(path);
}

std::shared_ptr<EPTio::HierarchyIndex>
EPTio::HierarchyIndex::build_metadata(const std::string& endpoint)
{
  auto idx = std::make_shared<HierarchyIndex>();
  idx->remote = is_remote(endpoint);

  std::string clean_endpoint = endpoint;
  size_t qpos = endpoint.find('?');
  if (qpos != std::string::npos) {
    idx->query_string = endpoint.substr(qpos);
    clean_endpoint = endpoint.substr(0, qpos);
  }

  idx->base_path = clean_endpoint;
  size_t pos = idx->base_path.rfind("ept.json");
  if (pos == std::string::npos)
    throw std::runtime_error("EPT endpoint must point to an ept.json file: " + endpoint);
  idx->base_path = idx->base_path.substr(0, pos);
  if (!idx->base_path.empty() && idx->base_path.back() != '/')
    idx->base_path += '/';

  // === Inline of parse_ept_json, writing into idx->* ===
  std::string json_str = read_file_contents_impl(idx->base_path + "ept.json" + idx->query_string);
  idx->ept_metadata = nlohmann::json::parse(json_str);

  std::string data_type = idx->ept_metadata.value("dataType", "");
  if (data_type != "laszip")
    throw std::runtime_error("EPT dataset uses '" + data_type + "' format, only 'laszip' is supported");

  auto bounds = idx->ept_metadata["bounds"];
  if (!bounds.is_array() || bounds.size() != 6)
    throw std::runtime_error("Invalid EPT bounds in ept.json");
  for (int i = 0; i < 6; i++) idx->cube_bounds[i] = bounds[i].get<double>();

  if (idx->ept_metadata.contains("boundsConforming")) {
    auto cbounds = idx->ept_metadata["boundsConforming"];
    if (cbounds.is_array() && cbounds.size() == 6) {
      for (int i = 0; i < 6; i++) idx->conf_bounds[i] = cbounds[i].get<double>();
    } else {
      for (int i = 0; i < 6; i++) idx->conf_bounds[i] = idx->cube_bounds[i];
    }
  } else {
    for (int i = 0; i < 6; i++) idx->conf_bounds[i] = idx->cube_bounds[i];
  }

  if (idx->ept_metadata.contains("srs")) {
    auto srs = idx->ept_metadata["srs"];
    idx->srs_wkt = srs.value("wkt", "");
    std::string authority = srs.value("authority", "");
    std::string horizontal = srs.value("horizontal", "");
    if (idx->srs_wkt.empty() && authority == "EPSG" && !horizontal.empty()) {
      try { idx->srs_epsg = std::stoi(horizontal); } catch (...) { idx->srs_epsg = 0; }
    }
  }

  // === Inline of find_probe_tile, writing idx->probe_tile_path ===
  std::deque<EPTkey> pages_to_visit;
  pages_to_visit.push_back(EPTkey(0, 0, 0, 0));
  while (!pages_to_visit.empty()) {
    EPTkey page_key = pages_to_visit.front();
    pages_to_visit.pop_front();

    std::string hier_path = idx->base_path + "ept-hierarchy/" +
      std::to_string(page_key.d) + "-" + std::to_string(page_key.x) + "-" +
      std::to_string(page_key.y) + "-" + std::to_string(page_key.z) + ".json" + idx->query_string;
    std::string h_str;
    try { h_str = read_file_contents_impl(hier_path); }
    catch (...) { continue; }
    nlohmann::json hierarchy = nlohmann::json::parse(h_str);

    for (auto& [key_str, value] : hierarchy.items()) {
      int pc = value.get<int>();
      if (pc <= 0) continue;
      int d, x, y, z;
      if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4) continue;
      EPTkey k(d, x, y, z);
      double cube_size = idx->cube_bounds[3] - idx->cube_bounds[0];
      double node_size = cube_size / (1 << d);
      std::string path = idx->base_path + "ept-data/" +
        std::to_string(d) + "-" + std::to_string(x) + "-" +
        std::to_string(y) + "-" + std::to_string(z) + ".laz" + idx->query_string;
      try {
        LASio probe;
        probe.open(path);
        probe.close();
        idx->probe_tile_path = path;
        return idx;
      } catch (...) { continue; }
    }
    for (auto& [key_str, value] : hierarchy.items()) {
      if (value.get<int>() != -1) continue;
      int d, x, y, z;
      if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4) continue;
      pages_to_visit.push_back(EPTkey(d, x, y, z));
    }
  }
  throw std::runtime_error("EPT dataset has no readable tiles to derive schema from: " + idx->base_path);
}

void EPTio::HierarchyIndex::ensure_tiles(
    const std::vector<std::array<double, 4>>& aoi_bboxes)
{
  // Full-walk cache short-circuit. AOI-filtered walks deliberately do not
  // consult or set tiles_built — see the header doc for the reuse contract.
  if (tiles_built && aoi_bboxes.empty()) return;

  // Reset before walking. `tiles` / `total_points` are append-only inside
  // the BFS; if a previous AOI-filtered walk left partial state, a later
  // call (any AOI scope, including the no-filter full walk) must start
  // fresh or it will double-count or silently drop nodes outside the prior
  // AOI from the canonical full enumeration.
  tiles.clear();
  total_points = 0;

  const double cube_size = cube_bounds[3] - cube_bounds[0];
  const bool has_aoi_filter = !aoi_bboxes.empty();

  // 2D-only AOI intersection: EPT octree z-extent is unbounded for our
  // purposes here, and a hierarchy node's bbox spans the full cube z-range
  // at depth 0, so we cull on xy only. This matches partition_ept's own
  // 2D cell-occupancy logic at the call site.
  auto key_intersects_aoi = [&](const EPTkey& k) -> bool {
    if (!has_aoi_filter) return true;
    const double node_size = cube_size / (double)(1 << k.d);
    const double nxmin = cube_bounds[0] + k.x * node_size;
    const double nymin = cube_bounds[1] + k.y * node_size;
    const double nxmax = nxmin + node_size;
    const double nymax = nymin + node_size;
    for (const auto& a : aoi_bboxes) {
      if (nxmax <= a[0] || nxmin >= a[2]) continue;
      if (nymax <= a[1] || nymin >= a[3]) continue;
      return true;
    }
    return false;
  };

  // Level-synchronized BFS with concurrent fetch at each level. The
  // sequential BFS paid one HTTP RTT per sub-page; on a real remote EPT
  // the hierarchy walk dominates first-read latency (~3 s for autzen-
  // classified). HTTP/2 multiplex (enabled by add_ept_endpoint) lets
  // many in-flight requests share one connection. Parsing and tile
  // accumulation stay single-threaded so `tiles` / `total_points` need
  // no locking.
  //
  // Bound the number of in-flight std::async tasks per level. Without a
  // cap, a fan-out node with hundreds of sub-pages would spawn that many
  // OS threads at once — likely to throw std::system_error or destroy
  // throughput via context-switch overhead. 8 matches our typical
  // ncpu_outer_loop budget.
  //
  // Workers must not call warning() / Rprintf directly: that touches R's
  // C API from a non-OpenMP thread. Errors are returned as text via the
  // result struct and surfaced on the consumer (main) thread.
  struct PageFetchResult {
    EPTkey key;
    std::string content;
    std::string warning_msg;       // emitted by consumer thread
    bool root_failure = false;     // root JSON missing → fatal
    std::string root_failure_msg;
  };

  constexpr size_t max_concurrent = 8;
  std::vector<EPTkey> frontier;
  frontier.push_back(EPTkey(0, 0, 0, 0));

  while (!frontier.empty()) {
    std::vector<EPTkey> next_frontier;

    for (size_t batch_start = 0; batch_start < frontier.size(); batch_start += max_concurrent) {
      const size_t batch_end = std::min(batch_start + max_concurrent, frontier.size());
      std::vector<std::future<PageFetchResult>> fetches;
      fetches.reserve(batch_end - batch_start);

      for (size_t i = batch_start; i < batch_end; ++i) {
        EPTkey k = frontier[i];
        // Out-of-AOI subtree: skip the JSON fetch entirely. All descendants
        // of an out-of-AOI parent are themselves out-of-AOI, so there's
        // nothing in this branch we'd want to enumerate.
        if (!key_intersects_aoi(k)) continue;
        fetches.push_back(std::async(std::launch::async,
          [this, k]() -> PageFetchResult {
            PageFetchResult r;
            r.key = k;
            std::string path = base_path + "ept-hierarchy/" +
              std::to_string(k.d) + "-" + std::to_string(k.x) + "-" +
              std::to_string(k.y) + "-" + std::to_string(k.z) + ".json" + query_string;
            try { r.content = read_file_contents_impl(path); return r; }
            catch (const std::exception& e) {
              const bool is_root = (k.d == 0 && k.x == 0 && k.y == 0 && k.z == 0);
              if (is_root) {
                r.root_failure = true;
                r.root_failure_msg = "Failed to read EPT hierarchy: " + path + ": " + e.what();
              } else {
                r.warning_msg = "EPT sub-hierarchy file not found: " + path;
              }
              return r;
            }
          }));
      }

      for (auto& fut : fetches) {
        PageFetchResult result = fut.get();
        if (result.root_failure)
          throw std::runtime_error(result.root_failure_msg);
        if (!result.warning_msg.empty())
          warning("%s\n", result.warning_msg.c_str());
        if (result.content.empty()) continue;

        nlohmann::json hierarchy = nlohmann::json::parse(result.content);
        for (auto& [key_str, value] : hierarchy.items()) {
          int d, x, y, z;
          if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4) continue;
          EPTkey k(d, x, y, z);
          // AOI prune at child level too: a fetched parent's JSON enumerates
          // up to 8 octree children — most may sit outside the AOI even when
          // the parent overlaps it. Dropping them here keeps `tiles` /
          // `next_frontier` proportional to the AOI footprint instead of
          // the full subtree.
          if (!key_intersects_aoi(k)) continue;
          int64_t pc = value.get<int64_t>();
          if (pc > 0) {
            TileEntry t;
            t.key = k;
            t.point_count = pc;
            double node_size = cube_size / (1 << d);
            t.xmin = cube_bounds[0] + x * node_size;
            t.ymin = cube_bounds[1] + y * node_size;
            t.zmin = cube_bounds[2] + z * node_size;
            t.xmax = t.xmin + node_size;
            t.ymax = t.ymin + node_size;
            t.zmax = t.zmin + node_size;
            tiles.push_back(t);
            total_points += pc;
          } else if (pc == -1) {
            next_frontier.push_back(k);
          }
        }
      }
    }
    frontier = std::move(next_frontier);
  }

  // Only the full walk is cacheable. An AOI-filtered run leaves a partial
  // `tiles` set scoped to the caller's AOI — a future un-filtered call must
  // be allowed to re-walk.
  if (!has_aoi_filter) tiles_built = true;
}

void EPTio::set_index(std::shared_ptr<const HierarchyIndex> idx)
{
  index = std::move(idx);
}

void EPTio::create(const std::string&)
{
  throw std::logic_error("EPT writing is not supported");
}

void EPTio::init(const Header*)
{
  throw std::logic_error("EPT writing is not supported");
}

bool EPTio::write_point(Point*)
{
  throw std::logic_error("EPT writing is not supported");
}

bool EPTio::is_opened()
{
  return opened;
}

int64_t EPTio::p_count()
{
  return points_read;
}

void EPTio::close()
{
  // Drain the prefetch worker before tearing down state — its lambda
  // captures `this` and can write into our queues if still running.
  cancel_prefetch();

  if (current_tile)
  {
    current_tile->close();
    delete current_tile;
    current_tile = nullptr;
#ifdef USING_GDAL
    if (!current_vsimem_path.empty())
      VSIUnlink(current_vsimem_path.c_str());
#endif
    current_vsimem_path.clear();
  }

  tile_queue.clear();
  opened = false;
  total_points = 0;
  points_read = 0;
}

void EPTio::reset_accessor()
{
  // No accessors to reset — EPTio delegates to LASio per tile
}

void EPTio::set_depth(int depth)
{
  depth_limit = depth;
}

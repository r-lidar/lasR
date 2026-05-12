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
#include <stdexcept>
#include <cmath>
#include <utility>

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

LASio* EPTio::open_tile_sync(const EPTkey& key)
{
  std::string path = tile_path(key);
  LASio* tile = new LASio();
  try
  {
    tile->open(path);
    Header temp_header;
    tile->populate_header(&temp_header);
    return tile;
  }
  catch (const std::exception& e)
  {
    warning("Failed to open EPT tile %s: %s\n", path.c_str(), e.what());
    delete tile;
    return nullptr;
  }
}

void EPTio::prefetch_next_tile()
{
  if (tile_queue.empty()) return;
  EPTkey key = tile_queue.front();
  tile_queue.pop_front();
  next_tile_future = std::async(std::launch::async,
    [this, key]() -> LASio* { return open_tile_sync(key); });
}

void EPTio::cancel_prefetch()
{
  if (!next_tile_future.valid()) return;
  // Drain the future and discard the result (cannot cancel std::async
  // mid-flight; we just wait it out and free the LASio if it succeeded).
  LASio* leftover = next_tile_future.get();
  if (leftover) {
    leftover->close();
    delete leftover;
  }
}

bool EPTio::open_next_tile()
{
  // Close previous tile
  if (current_tile)
  {
    current_tile->close();
    delete current_tile;
    current_tile = nullptr;
  }

  // 1) Was a tile prefetched in the background? Consume that first.
  if (next_tile_future.valid())
  {
    LASio* prefetched = next_tile_future.get();  // joins the worker
    if (prefetched)
    {
      current_tile = prefetched;
      // Kick off the next prefetch while the caller drains this tile.
      prefetch_next_tile();
      return true;
    }
    // Prefetch failed (warning already logged in open_tile_sync). Fall
    // through to the queue — there may still be more tiles to try.
  }

  // 2) Synchronous open from the queue (first tile, or after a prefetch
  //    failure with more tiles still queued).
  if (tile_queue.empty()) return false;

  EPTkey key = tile_queue.front();
  tile_queue.pop_front();
  current_tile = open_tile_sync(key);
  if (!current_tile) return open_next_tile();  // try the next one

  // Background-fetch the tile after this one so its bytes are
  // already arriving by the time we need it.
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

void EPTio::HierarchyIndex::ensure_tiles()
{
  if (tiles_built) return;

  const double cube_size = cube_bounds[3] - cube_bounds[0];

  // Level-synchronized BFS with concurrent fetch at each level. The
  // sequential BFS paid one HTTP RTT per sub-page; on a real remote EPT
  // the hierarchy walk dominates first-read latency (~3 s for autzen-
  // classified). HTTP/2 multiplex (enabled by add_ept_endpoint) lets
  // many in-flight requests share one connection. Parsing and tile
  // accumulation stay single-threaded so `tiles` / `total_points` need
  // no locking.
  std::vector<EPTkey> frontier;
  frontier.push_back(EPTkey(0, 0, 0, 0));

  while (!frontier.empty()) {
    std::vector<std::future<std::pair<EPTkey, std::string>>> fetches;
    fetches.reserve(frontier.size());

    for (const EPTkey& k : frontier) {
      fetches.push_back(std::async(std::launch::async,
        [this, k]() -> std::pair<EPTkey, std::string> {
          std::string path = base_path + "ept-hierarchy/" +
            std::to_string(k.d) + "-" + std::to_string(k.x) + "-" +
            std::to_string(k.y) + "-" + std::to_string(k.z) + ".json" + query_string;
          try { return {k, read_file_contents_impl(path)}; }
          catch (const std::exception& e) {
            const bool is_root = (k.d == 0 && k.x == 0 && k.y == 0 && k.z == 0);
            if (is_root) throw std::runtime_error("Failed to read EPT hierarchy: " + path + ": " + e.what());
            warning("EPT sub-hierarchy file not found: %s\n", path.c_str());
            return {k, ""};
          }
        }));
    }

    std::vector<EPTkey> next_frontier;
    for (auto& fut : fetches) {
      std::pair<EPTkey, std::string> result = fut.get();
      if (result.second.empty()) continue;

      nlohmann::json hierarchy = nlohmann::json::parse(result.second);
      for (auto& [key_str, value] : hierarchy.items()) {
        int d, x, y, z;
        if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4) continue;
        EPTkey k(d, x, y, z);
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
    frontier = std::move(next_frontier);
  }

  tiles_built = true;
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

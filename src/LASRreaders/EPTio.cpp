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
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cmath>

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

bool EPTio::open_next_tile()
{
  // Close previous tile
  if (current_tile)
  {
    current_tile->close();
    delete current_tile;
    current_tile = nullptr;
  }

  // No more tiles
  if (tile_queue.empty())
    return false;

  EPTkey key = tile_queue.front();
  tile_queue.pop_front();

  std::string path = tile_path(key);
  current_tile = new LASio();

  try
  {
    current_tile->open(path);
    // populate_header initializes LASlib's point reader and extrabytes accessors
    Header temp_header;
    current_tile->populate_header(&temp_header);
  }
  catch (const std::exception& e)
  {
    warning("Failed to open EPT tile %s: %s\n", path.c_str(), e.what());
    delete current_tile;
    current_tile = nullptr;
    // Try next tile
    return open_next_tile();
  }

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

std::string EPTio::read_file_contents(const std::string& path) const
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

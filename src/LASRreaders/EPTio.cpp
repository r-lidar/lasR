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

static bool is_remote(const std::string& path)
{
#ifdef USING_GDAL
  if (path.compare(0, 7, "http://") == 0) return true;
  if (path.compare(0, 8, "https://") == 0) return true;
  if (path.compare(0, 9, "/vsicurl/") == 0) return true;
  if (path.compare(0, 7, "/vsis3/") == 0) return true;
  if (path.compare(0, 7, "/vsigs/") == 0) return true;
  if (path.compare(0, 7, "/vsiaz/") == 0) return true;
  if (path.compare(0, 9, "/vsiadls/") == 0) return true;
  if (path.compare(0, 8, "/vsioss/") == 0) return true;
  if (path.compare(0, 10, "/vsiswift/") == 0) return true;
#else
  (void)path;
#endif
  return false;
}

EPTio::EPTio()
{
  remote = false;
  opened = false;
  span = 0;
  srs_epsg = 0;
  depth_limit = -1;
  total_points = 0;
  current_tile = nullptr;
  points_read = 0;
  x_scale = y_scale = z_scale = 0.001;
  x_offset = y_offset = z_offset = 0;
  scale_offset_initialized = false;
  has_gps = false;
  has_rgb = false;
  has_nir = false;

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
  read_root_tile_header();
}

void EPTio::read_root_tile_header()
{
  // Find the first tile with points from the root hierarchy to read scale/offset
  std::string hier_path = base_path + "ept-hierarchy/0-0-0-0.json" + query_string;
  std::string json_str;

  try { json_str = read_file_contents(hier_path); }
  catch (...) { return; }

  nlohmann::json hierarchy = nlohmann::json::parse(json_str);

  for (auto& [key_str, value] : hierarchy.items())
  {
    int point_count = value.get<int>();
    if (point_count <= 0) continue;

    // Found a tile with points — try to open it via LASio (no direct LASlib)
    int d, x, y, z;
    if (sscanf(key_str.c_str(), "%d-%d-%d-%d", &d, &x, &y, &z) != 4)
      continue;

    EPTkey key(d, x, y, z);
    std::string path = tile_path(key);

    try
    {
      LASio probe;
      probe.open(path);
      Header h;
      probe.populate_header(&h);
      x_scale = h.x_scale_factor;
      y_scale = h.y_scale_factor;
      z_scale = h.z_scale_factor;
      x_offset = h.x_offset;
      y_offset = h.y_offset;
      z_offset = h.z_offset;
      scale_offset_initialized = true;
      probe.close();
      return;
    }
    catch (...) { continue; }
  }
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

  // Read span
  span = ept_metadata.value("span", 256);

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

  // Parse schema to determine point format
  has_gps = false;
  has_rgb = false;
  has_nir = false;

  if (ept_metadata.contains("schema"))
  {
    for (const auto& dim : ept_metadata["schema"])
    {
      std::string name = dim.value("name", "");
      if (name == "GpsTime") has_gps = true;
      if (name == "Red") has_rgb = true;
      if (name == "Infrared") has_nir = true;
    }
  }
}

void EPTio::populate_header(Header* header, bool)
{
  if (!opened)
    throw std::logic_error("Internal error: EPTio not opened");

  header->signature = "EPTF";
  header->version_major = 1;
  header->version_minor = 0;

  // Bounds from conforming bounds
  header->min_x = conf_bounds[0];
  header->min_y = conf_bounds[1];
  header->min_z = conf_bounds[2];
  header->max_x = conf_bounds[3];
  header->max_y = conf_bounds[4];
  header->max_z = conf_bounds[5];

  // CRS
  if (!srs_wkt.empty())
    header->set_crs(srs_wkt);
  else if (srs_epsg > 0)
    header->set_crs(srs_epsg);

  // Determine point data format from schema
  int pdf = LASio::guess_point_data_format(has_gps, has_rgb, has_nir, false);
  header->point_data_format = pdf;

  // Use scale/offset from root tile for coordinate consistency
  header->x_scale_factor = x_scale;
  header->y_scale_factor = y_scale;
  header->z_scale_factor = z_scale;
  header->x_offset = x_offset;
  header->y_offset = y_offset;
  header->z_offset = z_offset;

  // Build schema matching the LAS point format
  header->schema.add_attribute("flags", AttributeType::UINT8, 1, 0, "Internal 8-bit mask reserved for lasR core engine");
  header->schema.add_attribute("X", AttributeType::INT32, header->x_scale_factor, header->x_offset, "X coordinate");
  header->schema.add_attribute("Y", AttributeType::INT32, header->y_scale_factor, header->y_offset, "Y coordinate");
  header->schema.add_attribute("Z", AttributeType::INT32, header->z_scale_factor, header->z_offset, "Z coordinate");
  header->schema.add_attribute("Intensity", AttributeType::UINT16, 1, 0, "Pulse return magnitude");
  header->schema.add_attribute("ReturnNumber", AttributeType::UINT8, 1, 0, "Pulse return number");
  header->schema.add_attribute("NumberOfReturns", AttributeType::UINT8, 1, 0, "Total number of returns for a given pulse");
  header->schema.add_attribute("Classification", AttributeType::UINT8, 1, 0, "Classification of the point");
  header->schema.add_attribute("UserData", AttributeType::UINT8, 1, 0, "User data");
  header->schema.add_attribute("PointSourceID", AttributeType::INT16, 1, 0, "Point source ID");

  if (pdf >= 6)
  {
    header->schema.add_attribute("ScanAngle", AttributeType::FLOAT, 1, 0, "Scan angle");
    header->schema.add_attribute("ScannerChannel", AttributeType::UINT8, 1, 0, "Scanner channel");
  }
  else
  {
    header->schema.add_attribute("ScanAngle", AttributeType::INT8, 1, 0, "Scan angle rank");
  }

  if (has_gps)
    header->schema.add_attribute("gpstime", AttributeType::DOUBLE, 1, 0, "GPS time");

  if (has_rgb)
  {
    header->schema.add_attribute("R", AttributeType::UINT16, 1, 0, "Red channel");
    header->schema.add_attribute("G", AttributeType::UINT16, 1, 0, "Green channel");
    header->schema.add_attribute("B", AttributeType::UINT16, 1, 0, "Blue channel");
  }

  if (has_nir)
    header->schema.add_attribute("NIR", AttributeType::UINT16, 1, 0, "Near infrared channel");

  header->schema.add_attribute("EdgeOfFlightline", AttributeType::BIT, 1, 0, "Edge of flight line");
  header->schema.add_attribute("ScanDirectionFlag", AttributeType::BIT, 1, 0, "Scan direction flag");
  header->schema.add_attribute("Synthetic", AttributeType::BIT, 1, 0, "Synthetic flag");
  header->schema.add_attribute("Keypoint", AttributeType::BIT, 1, 0, "Keypoint flag");
  header->schema.add_attribute("Withheld", AttributeType::BIT, 1, 0, "Withheld flag");

  if (pdf >= 6)
    header->schema.add_attribute("Overlap", AttributeType::BIT, 1, 0, "Overlap flag");

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
#ifdef USING_GDAL
  if (is_remote(path))
  {
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
  }
#else
  if (is_remote(path))
    throw std::runtime_error("Remote EPT endpoints require GDAL support: " + path);
#endif

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

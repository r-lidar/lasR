// LASR
#include "LAScatalog.h"
#include "Progress.h"
#include "error.h"
#include "Grid.h"

// LASlib
#include "lasreader.hpp"
#include "laskdtree.hpp"

// STL
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>
#include <ctime>

// To parse JSON VPC
#include <nlohmann/json.hpp>
#include <fstream>

// Define macros for cross-platform compatibility
#ifdef _WIN32
#define timegm _mkgmtime
inline void gmtime_r(const time_t* timep, std::tm* result)
{
  gmtime_s(result, timep);
}
#endif

bool LAScatalog::read(const std::vector<std::string>& files, bool progress)
{
  Progress pb;
  pb.set_total(files.size());
  pb.set_prefix("Read files headers");
  pb.set_display(progress);

  for (auto& file : files)
  {
    pb++;
    pb.show();
    PathType type = parse_path(file);

    // A LAS or LAZ file
    if (type == PathType::LASFILE)
    {
      add_file(file);
    }
    // A virtual point cloud file
    else if (type == PathType::VPCFILE)
    {
      if (files.size() > 1)
      {
        last_error = "Virtual point cloud file detected mixed with other content";
        return false;
      }
      if (!read_vpc(file))
      {
        return false;
      }
    }
    else if (type == PathType::LAXFILE) // # nocov
    {
      // nothing to do, just skip
    }
    else if (type == PathType::DIRECTORY)
    {
      for (const auto& entry : std::filesystem::directory_iterator(file))
      {
        if (entry.is_regular_file())
        {
          std::string ext = entry.path().extension().string();
          if (ext == ".las" || ext == ".laz" || ext == ".LAS" || ext == ".LAZ")
          {
            add_file(entry.path().string());
          }
        }
      }
    }
    else if (type == PathType::MISSINGFILE)
    {
      last_error = "File not found: " + file;
      return false;
    }
    else
    {
      last_error = "Unknown file type: " + file;
      return false;
    }
  }

  pb.done();

  if (epsg_set.size() > 1) warning("mix epsg found. First one retained\n");
  if (wkt_set.size() > 1) warning("mix wkt crs found. First one retained.\n");
  if (epsg_set.size() > 0) crs = CRS(*epsg_set.begin());
  if (wkt_set.size() > 0) crs = CRS(*wkt_set.begin());

  return true;
}

bool LAScatalog::read_vpc(const std::string& filename)
{
  clear();
  use_dataframe = false;
  use_vpc = true;

  // Get the parent file to resolve relative path later
  std::filesystem::path parent_folder = std::filesystem::path(filename).parent_path();

  try
  {
    // read the JSON file
    std::ifstream file(filename);
    nlohmann::json vpc = nlohmann::json::parse(file);

    // Check that it has a FeatureCollection property
    if (vpc.find("type") == vpc.end() || vpc["type"] != "FeatureCollection")
    {
      last_error = "The input file is not a virtual point cloud file"; // # nocov
      return false; // # nocov
    }

    // Check that it has a feature property
    if (vpc.find("features") == vpc.end())
    {
      last_error = "Missing 'features' key in the virtual point cloud file"; // # nocov
      return false; // # nocov
    }

    // Loop over the features to read the file paths and bounding boxes
    for (const auto& feature : vpc["features"])
    {
      if (!feature.contains("type") || !feature.contains("stac_version") || !feature.contains("assets") || !feature.contains("properties"))
      {
        last_error = "Malformed virtual point cloud file: missing properties"; // # nocov
        return false; // # nocov
      }

      if (feature["type"] != "Feature")
      {
        last_error = "Malformed virtual point cloud file: 'type' is not equal to 'Feature'"; // # nocov
        return false; // # nocov
      }

      if (feature["assets"].empty() || feature["properties"].empty())
      {
        last_error = "Malformed virtual point cloud file: empty 'assets' or 'properties"; // # nocov
        return false; // # nocov
      }

      if (feature["stac_version"] != "1.0.0")
      {
        last_error = "Unsupported STAC version: " + feature["stac_version"].get<std::string>(); // # nocov
        return false; // # nocov
      }

      // Find the path to the file and resolve relative path
      auto first_asset = *feature["assets"].begin();
      std::string file_path = first_asset["href"];
      file_path = std::filesystem::weakly_canonical(parent_folder / file_path).string();

      int pccount = feature["properties"]["pc:count"].get<int>();

      // Get the CRS either in WKT or EPSG
      std::string wkt = feature["properties"].value("proj:wkt2", "");
      int epsg = feature["properties"].value("proj:epsg", 0);

      // Read the bounding box. If not found fall back to regular file reading
      auto optbbox = feature["properties"].find("proj:bbox");
      if (optbbox == feature["properties"].end())
      {
        // # nocov start
        if (!add_file(file_path))
        {
          return false;
        }
        continue;
        // # nocov end
      }

      // We are sure there is a bbox
      auto bbox = feature["properties"]["proj:bbox"];
      double min_x, min_y, max_x, max_y;
      if (bbox.size() == 6)
      {
        min_x = bbox[0].get<double>();
        min_y = bbox[1].get<double>();
        max_x = bbox[3].get<double>();
        max_y = bbox[4].get<double>();
      }
      else if (bbox.size() == 4)
      {
        min_x = bbox[0].get<double>();
        min_y = bbox[1].get<double>();
        max_x = bbox[2].get<double>();
        max_y = bbox[3].get<double>();
      }
      else
      {
        last_error = "Malformed virtual point cloud file: proj:bbox should be 2D or 3D"; // # nocov
        return false; // # nocov
      }

      bool spatial_index = feature["properties"].value("index:indexed", false); // backward compatibility

      files.push_back(file_path);
      add_wkt(wkt);
      add_epsg(epsg);
      add_bbox(min_x, min_y, max_x, max_y, spatial_index);
      npoints.push_back(pccount);
    }
  }
  catch (const std::ifstream::failure& e)
  {
    last_error = std::string("Failed to read JSON file: ") + e.what(); // # nocov
    return false; // # nocov
  }
  catch (const nlohmann::json::parse_error& e)
  {
    last_error = std::string("JSON parsing error: ") + e.what();
    return false;
  }
  catch (const std::exception& e)
  {
    last_error = std::string("An unexpected error occurred in read_vpc(): ") + e.what(); // # nocov
    return false; // # nocov
  }

  return true;
}

bool LAScatalog::write_vpc(const std::string& vpcfile, const CRS& crs, bool absolute_path, bool use_gpstime)
{
  if (use_dataframe)
  {
    last_error = "Cannot write a virtual point cloud file with a data.frame"; // # nocov
    return false; // # nocov
  }

  if (!crs.is_valid())
  {
    last_error = "Invalid CRS. Cannot write a VPC file";
    return false;
  }

  std::filesystem::path output_path = vpcfile;

  if (output_path.extension() != ".vpc")
  {
    last_error = "The virtual point cloud must have the extension '.vpc'"; // # nocov
    return false; // # nocov
  }

  output_path = output_path.parent_path();

  std::ofstream output(vpcfile);
  if (!output.good())
  {
    last_error = std::string("Failed to create file: ") + vpcfile; // # nocov
    return false; // # nocov
  }

  std::string wkt = crs.get_wkt();

  std::ostringstream oss;
  oss << std::quoted(wkt);
  wkt = oss.str();
  int epsg = crs.get_epsg();

  // Mmmm.... boost property_tree is not able to write numbers... will do everything by hand
  auto autoquote = [](const std::string& str) { return '"' + str + '"'; };

  output << "{" << std::endl;
  output << "  \"type\": \"FeatureCollection\"," << std::endl;
  output << "  \"features\": [" << std::endl;

  for (int i = 0 ; i < get_number_files() ; i++)
  {
    std::filesystem::path file = files[i];
    std::string relative_path = file.string();
    if (!absolute_path)
    {
      std::string relative = std::filesystem::relative(file, output_path).string();
      if (!relative.empty())
      {
        relative_path = "./" + relative;
        std::replace(relative_path.begin(), relative_path.end(), '\\', '/'); // avoid problem of backslash on windows
      }
    }

    Rectangle& bbox = bboxes[i];
    uint64_t n = npoints[i];
    bool index = indexed[i];
    double zmin = zlim[i].first;
    double zmax = zlim[i].second;

    std::string date;
    int year;
    int doy;

    if (use_gpstime)
    {
      if (gpstime_encodind_bits[i])
      {
        if (gpstime_dates.size() == 0)
        {
          warning("This files as no GPS time. Cannot use GPS time to assign a date.");
          year = header_dates[i].first;
          doy = header_dates[i].second;
        }
        else
        {
          year = gpstime_dates[i].first;
          doy = gpstime_dates[i].second;
        }
      }
      else
      {
        warning("The GPS time is not recorded as Adjusted Standard GPS Time but as GPS Week Time. Cannot use GPS time to assign a date.");
        year = header_dates[i].first;
        doy = header_dates[i].second;
      }
    }
    else
    {
      year = header_dates[i].first;
      doy = header_dates[i].second;
    }

    if (year > 0)
    {
      // Initialize the tm structure to represent January 1st of the given year
      std::tm timeinfo = {};
      timeinfo.tm_year = year - 1900;
      timeinfo.tm_mon = 0; // January
      timeinfo.tm_mday = 1; // 1st day

      // Convert tm structure to time_t (seconds since epoch)
      time_t first_day_of_year = timegm(&timeinfo);

      // Add the day of the year (doy - 1) days to the first day of the year
      time_t desired_day = first_day_of_year + doy * 86400; // 86400 seconds in a day

      // Convert the resulting time_t back to tm structure
      gmtime_r(&desired_day, &timeinfo);

      // Format the date
      char buffer[26];
      strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
      date = std::string(buffer);date = std::string(buffer);
    }
    else
    {
      date = "0-01-01T00:00:00Z";
    }

    PointXY A = {bbox.minx, bbox.miny};
    PointXY B = {bbox.maxx, bbox.miny};
    PointXY C = {bbox.maxx, bbox.maxy};
    PointXY D = {bbox.minx, bbox.maxy};
    OGRSpatialReference oTargetSRS;
    OGRSpatialReference oSourceSRS;
    oTargetSRS.importFromEPSG(4979);
    oTargetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    oSourceSRS = crs.get_crs();
    OGRCoordinateTransformation *poTransform = OGRCreateCoordinateTransformation(&oSourceSRS, &oTargetSRS);
    double z = 0;
    if (!poTransform->Transform(1, &A.x, &A.y, &zmin) ||
        !poTransform->Transform(1, &B.x, &B.y, &z) ||
        !poTransform->Transform(1, &C.x, &C.y, &zmax) ||
        !poTransform->Transform(1, &D.x, &D.y, &z))
    {
      last_error = "Transformation of the bounding in WGS 84 failed!";
      return false;
    }

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "[ [%.9lf,%.9lf,%.3lf], [%.9lf,%.9lf,%.3lf], [%.9lf,%.9lf,%.3lf], [%.9lf,%.9lf,%.3lf], [%.9lf,%.9lf,%.3lf] ]", A.x, A.y, zmin, B.x, B.y, zmin, C.x, C.y, zmax, D.x, D.y, zmax, A.x, A.y, zmin);
    std::string geometry(buffer);
    snprintf(buffer, sizeof(buffer), "[%.9lf, %.9lf, %.3lf, %.9lf, %.9lf, %.3lf]", MIN(A.x, D.x), MIN(A.y, B.y), zmin, MAX(B.x, C.x), MAX(C.y, D.y), zmax);
    std::string sbbox(buffer);

    output << "  {" << std::endl;
    output << "    \"type\": \"Feature\"," << std::endl;
    output << "    \"stac_version\": \"1.0.0\"," << std::endl;
    output << "    \"stac_extensions\": [" << std::endl;
    output << "      \"https://stac-extensions.github.io/pointcloud/v1.0.0/schema.json\"," << std::endl;
    output << "      \"https://stac-extensions.github.io/projection/v1.1.0/schema.json\"" << std::endl;
    output << "     ]," << std::endl;
    output << "    \"id\": " << autoquote(file.stem().string()) << "," << std::endl;
    output << "    \"geometry\": {" << std::endl;
    output << "      \"coordinates\": [" << std::endl;
    output << "        " << geometry << std::endl;
    output << "      ]," << std::endl;
    output << "      \"type\": \"Polygon\"" << std::endl;
    output << "    }," << std::endl;
    output << "    \"bbox\": " << sbbox << "," << std::endl;
    output << "    \"properties\": {" << std::endl;
    output << "      \"datetime\": " << autoquote(date) << "," << std::endl;
    output << "      \"pc:count\": " << n << "," << std::endl;;
    output << "      \"pc:type\": " << "\"lidar\"" << ","<< std::endl;
    if (index) output << "      \"index:indexed\": " << "true," << std::endl;
    output << "      \"proj:bbox\": [" << std::fixed << std::setprecision(3) << bbox.minx << ", " << bbox.miny << ", " << bbox.maxx << ", " << bbox.maxy << "],"<< std::endl;
    if (!wkt.empty()) output << "      \"proj:wkt2\": " << wkt << "," << std::endl;
    if (epsg != 0) output << "      \"proj:epsg\": " << epsg << std::endl;
    output << "    }," << std::endl;
    output << "    \"links\": []," << std::endl;
    output << "    \"assets\": {" << std::endl;
    output << "      \"data\": {" << std::endl;
    output << "      \"href\": " << autoquote(relative_path) << "," << std::endl;
    output << "      \"roles\": [\"data\"]" << std::endl;
    output << "      }" << std::endl;
    output << "    }" << std::endl;
    output << "  }";

    if (i < (int)files.size()-1)
      output << ",";

    output << std::endl;
  }

  output << "  ]" << std::endl;
  output << "}";

  output.close();

  return true;
}

void LAScatalog::add_bbox(double xmin, double ymin, double xmax, double ymax, bool indexed, bool noprocess)
{
  Rectangle bb(xmin, ymin, xmax, ymax);
  bboxes.push_back(bb);

  if (this->xmin > xmin) this->xmin = xmin;
  if (this->ymin > ymin) this->ymin = ymin;
  if (this->xmax < xmax) this->xmax = xmax;
  if (this->ymax < ymax) this->ymax = ymax;

  laskdtree->add(xmin, ymin, xmax, ymax);
  this->indexed.push_back(indexed);
  this->noprocess.push_back(noprocess);
}

void LAScatalog::add_crs(const LASheader* header)
{
  if (header->vlr_geo_keys)
  {
    for (int j = 0; j < header->vlr_geo_keys->number_of_keys; j++)
    {
      if (header->vlr_geo_key_entries[j].key_id == 3072)
      {
        add_epsg(header->vlr_geo_key_entries[j].value_offset);
        break;
      }
    }
  }

  if (header->vlr_geo_ogc_wkt)
  {
    for (unsigned int j = 0; j < header->number_of_variable_length_records; j++)
    {
      if (header->vlrs[j].record_id == 2112)
      {
        char* data = (char*)header->vlrs[j].data;
        int len = strnlen(data, header->vlrs[j].record_length_after_header);
        std::string wkt(data, len);
        add_wkt(wkt);
        break;
      }
    }
  }
}

bool LAScatalog::add_file(std::string file, bool noprocess)
{
  std::replace(file.begin(), file.end(), '\\', '/' );

  LASreadOpener lasreadopener;
  lasreadopener.add_file_name(file.c_str());
  LASreader* lasreader = lasreadopener.open();
  if (lasreader == 0)
  {
    last_error = "cannot open not open lasreader"; // # nocov
    return false; // # nocov
  }

  files.push_back(file);
  add_crs(&lasreader->header);
  add_bbox(lasreader->header.min_x, lasreader->header.min_y, lasreader->header.max_x, lasreader->header.max_y, lasreader->get_index() || lasreader->get_copcindex(), noprocess);
  npoints.push_back(MAX(lasreader->header.number_of_point_records, lasreader->header.extended_number_of_point_records));
  header_dates.push_back({lasreader->header.file_creation_year, lasreader->header.file_creation_day});
  zlim.push_back({lasreader->header.min_z, lasreader->header.max_z});
  gpstime_encodind_bits.push_back(lasreader->header.get_global_encoding_bit(0));

  lasreader->read_point();
  if (lasreader->point.have_gps_time && lasreader->header.get_global_encoding_bit(0) == true)
  {
    uint64_t ns = ((uint64_t)lasreader->point.get_gps_time()+1000000000ULL)*1000000000ULL + 315964800000000000ULL; // offset between gps epoch and unix epoch is 315964800 seconds

    struct timespec ts;
    ts.tv_sec = ns / 1000000000ULL;
    ts.tv_nsec = ns % 1000000000ULL;

    struct tm stm;
    gmtime_r(&ts.tv_sec, &stm);

    gpstime_dates.push_back({stm.tm_year + 1900, stm.tm_yday});

    //std::cout << stm.tm_year + 1900 << "-" << stm.tm_mon + 1 << "-" << stm.tm_mday << " " << stm.tm_hour << ":" << stm.tm_min << ":" << stm.tm_sec << std::endl;
  }
  else
  {
    gpstime_dates.push_back({0, 0});
  }

  lasreader->close();
  delete lasreader;

  use_dataframe = false;
  return true;
}

void LAScatalog::add_wkt(const std::string& wkt)
{
  if (!wkt.empty()) wkt_set.insert(wkt);
}

void LAScatalog::add_epsg(int epsg)
{
  if (epsg != 0) epsg_set.insert(epsg);
}

void LAScatalog::add_query(double xmin, double ymin, double xmax, double ymax)
{
  Rectangle* rect = new Rectangle(xmin, ymin, xmax, ymax);
  queries.push_back(rect);
}

void LAScatalog::add_query(double xcenter, double ycenter, double radius)
{
  Circle* circ = new Circle(xcenter, ycenter, radius);
  queries.push_back(circ);
}

bool LAScatalog::set_noprocess(const std::vector<bool>& b)
{
  if (b.size() != files.size())
  {
    last_error = "the vector indicating if a file is used as buffer only is not the same size as the vector of files"; // # nocov
    return false; // # nocov
  }

  noprocess = b;
  return true;
}

bool LAScatalog::set_chunk_size(double size)
{
  chunk_size = 0;

  if (size > 0)
  {
    if (queries.size() > 0)
    {
      last_error = "Impossible to set chunk size with queries";
      return false;
    }

    if (!laskdtree->was_built())
    {
      last_error = "internal error: laskdtree not built"; // # nocov
      return false; // # nocov
    }

    chunk_size = size;

    Grid grid(xmin, ymin, xmax, ymax, chunk_size);
    for (int i = 0 ; i < grid.get_ncells() ; i++)
    {
      double x = grid.x_from_cell(i);
      double y = grid.y_from_cell(i);
      double hsize = size/2;

      laskdtree->overlap(x-hsize, y-hsize, x+hsize, y+hsize);
      if (laskdtree->has_overlaps())
        add_query(x-hsize, y-hsize, x+hsize, y+hsize);
    }
  }

  return true;
}

bool LAScatalog::get_chunk(int i, Chunk& chunk) const
{
  if (i < 0 || i > get_number_chunks())
  {
    last_error = "chunk request out of bounds"; // # nocov
    return false; // # nocov
  }

  if (!laskdtree->was_built())
  {
    last_error = "internal error: spatial index of tile not built"; // # nocov
    return false; // # nocov
  }

  bool success = false;

  // Critical section because laskdtree->overlap is not thread safe
  #pragma omp critical(getchunk)
  {
    if (queries.size() == 0)
    {
      success = get_chunk_regular(i, chunk);
      chunk.process = !noprocess[i];
    }
    else
    {
      success = get_chunk_with_query(i, chunk);
      chunk.process = true;
    }
  }

  chunk.id = i;

  return success;
}

const std::vector<std::filesystem::path>& LAScatalog::get_files() const
{
  return files;
}

bool LAScatalog::get_chunk_regular(int i, Chunk& chunk) const
{
  chunk.clear();

  Rectangle bb = bboxes[i];
  chunk.xmin = bb.xmin();
  chunk.ymin = bb.ymin();
  chunk.xmax = bb.xmax();
  chunk.ymax = bb.ymax();

  if (!use_dataframe)
  {
    chunk.main_files.push_back(files[i].string());
    chunk.name = files[i].stem().string();
  }
  else
  {
    chunk.main_files.push_back("data.frame");
    chunk.name = "data.frame";
  }

  // We are working with an R data.frame: there is no file and especially
  // no neighbouring files. We can exit now.
  if (use_dataframe)
  {
    return true;
  }

  chunk.buffer = buffer;

  // Perform a query to find the files that encompass the buffered region
  unsigned int index;
  laskdtree->overlap(bb.xmin() - buffer, bb.ymin() - buffer, bb.xmax() + buffer, bb.ymax() + buffer);
  if (laskdtree->has_overlaps())
  {
    while (laskdtree->get_overlap(index))
    {
      std::string file = files[index].string();
      if (chunk.main_files[0] != file)
      {
        chunk.neighbour_files.push_back(file);
      }
    }
  }

  return true;
}

bool LAScatalog::get_chunk_with_query(int i, Chunk& chunk) const
{
  if (!laskdtree->was_built())
  {
    last_error = "internal error: laskdtree not built"; // # nocov
    return false; // # nocov
  }

  unsigned int index;
  chunk.clear();

  // Some shape are provided. We are performing queries i.e not processing the entire collection file by file
  Shape* q = queries[i];
  double minx = q->xmin();
  double miny = q->ymin();
  double maxx = q->xmax();
  double maxy = q->ymax();
  double centerx = q->centroid().x;
  double centery = q->centroid().y;
  double epsilon = 1e-8;

  // Search if there is a match
  laskdtree->overlap(minx - buffer, miny - buffer,  maxx + buffer, maxy + buffer);
  if (!laskdtree->has_overlaps())
  {
    char buff[64];
    snprintf(buff, sizeof(buff), "cannot find any file in [%.1lf, %.1lf %.1lf, %.1lf]", minx, miny,  maxx, maxy);
    last_error = buff;
    return false;
  }

  // There is a match we can update the chunk
  chunk.xmin = minx;
  chunk.ymin = miny;
  chunk.xmax = maxx;
  chunk.ymax = maxy;
  if (chunk.xmin < xmin) chunk.xmin = xmin;
  if (chunk.xmax > xmax) chunk.xmax = xmax;
  if (chunk.ymin < ymin) chunk.ymin = ymin;
  if (chunk.ymax > ymax) chunk.ymax = ymax;
  chunk.buffer = buffer;
  chunk.shape = q->type();

  // With an R data.frame there is no file and thus no neighbouring files. We can exit.
  if (use_dataframe)
  {
    chunk.main_files.push_back("dataframe");
    chunk.name = "data.frame";
    return true;
  }

  // We are working with a single file there is no neighbouring files. We can exit.
  if (get_number_files() == 1)
  {
    chunk.main_files.push_back(files[0].string());
    chunk.name = files[0].stem().string() + "_" + std::to_string(i);
    return true;
  }

  // There are likely multiple files that intersect the query. If there is only one it is easy. We can exit.
  if (laskdtree->get_num_overlaps() == 1)
  {
    laskdtree->get_overlap(index);
    chunk.main_files.push_back(files[index].string());
    chunk.name = files[index].stem().string() + "_" + std::to_string(i);
    return true;
  }

  // There are multiple files. All the files are part of the main
  while (laskdtree->get_overlap(index))
  {
    chunk.main_files.push_back(files[index].string());
    if (chunk.name.empty()) chunk.name = files[index].stem().string() + "_" + std::to_string(i);
  }

  // We search the file that contains the centroid of the query to assign a name to the query
  // If we can't find it we have already assigned a name anyway.
  laskdtree->overlap(centerx - epsilon, centery - epsilon,  centerx + epsilon, centery + epsilon);
  if (laskdtree->has_overlaps())
  {
    laskdtree->get_overlap(index);
    chunk.name = files[index].stem().string() + "_" + std::to_string(i);
  }

  // We perform a query again with buffered shape to get the other files in the buffer
  if (chunk.buffer > 0)
  {
    laskdtree->overlap(minx - buffer, miny - buffer, maxx + buffer, maxy + buffer);
    while (laskdtree->get_overlap(index))
    {
      std::string file = files[index].string();

      // if the file is not part of the main files
      auto it = std::find(chunk.main_files.begin(), chunk.main_files.end(), file);
      if (it != chunk.main_files.end())
      {
        chunk.neighbour_files.push_back(file);
      }
    }
  }

  return true;
}

void LAScatalog::build_index()
{
  if (!laskdtree->was_built()) laskdtree->build();
}

bool LAScatalog::check_spatial_index()
{
  bool multi_files = get_number_files() > 1;
  bool use_buffer = get_buffer() > 0;
  bool no_index = get_number_indexed_files() != get_number_files();
  bool has_queries = queries.size() > 0;
  return !((multi_files && use_buffer && no_index) || (has_queries && no_index));
}

int LAScatalog::get_number_chunks() const
{
  return (queries.size() == 0) ? get_number_files() : queries.size();
}

int LAScatalog::get_number_files() const
{
  return indexed.size();
}

int LAScatalog::get_number_indexed_files() const
{
  return std::count(indexed.begin(), indexed.end(), true);
}

void LAScatalog::set_all_indexed()
{
  std::fill(indexed.begin(), indexed.end(), true);
}

void LAScatalog::clear()
{
  xmin = F64_MAX;
  ymin = F64_MAX;
  xmax = F64_MIN;
  ymax = F64_MIN;
  last_error.clear();

  // CRS
  wkt_set.clear();
  epsg_set.clear();
  //epsg = 0;
  //wkt.clear();

  use_dataframe = true;
  use_vpc = false;

  buffer = 0;
  chunk_size = 0;

  indexed.clear();
  npoints.clear();
  noprocess.clear();
  bboxes.clear();
  files.clear();

  if (laskdtree) delete laskdtree;
  laskdtree = new LASkdtreeRectangles;
  laskdtree->init();

  for (auto p : queries) delete p;
  queries.clear();
}

bool LAScatalog::file_exists(std::string& file)
{
  auto it = std::find(files.begin(), files.end(), file);
  return it != files.end();
}

PathType LAScatalog::parse_path(const std::string& path)
{
  std::filesystem::path file_path(path);

  if (std::filesystem::exists(file_path))
  {
    if (std::filesystem::is_regular_file(file_path))
    {
      std::string ext = file_path.extension().string();
      if (ext == ".vpc" || ext == ".vpc") return PathType::VPCFILE;
      if (ext == ".las" || ext == ".laz" || ext == ".LAS" || ext == ".LAZ") return PathType::LASFILE;
      if (ext == ".lax" || ext == ".LAX") return PathType::LAXFILE;
      return PathType::OTHERFILE;
    }
    else if (std::filesystem::is_directory(path))
    {
      return PathType::DIRECTORY;
    }
    else
    {
      return PathType::UNKNOWNFILE;
    }
  }
  else
  {
    return PathType::MISSINGFILE;
  }
}


LAScatalog::LAScatalog()
{
  laskdtree = 0;
  clear();
}

LAScatalog::~LAScatalog()
{
  if (laskdtree) delete laskdtree;
  for (auto p : queries) delete p;
}

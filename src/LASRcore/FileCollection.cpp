// LASR
#include "FileCollection.h"
#include "Progress.h"
#include "error.h"
#include "print.h"
#include "Grid.h"
#include "PointSchema.h"

// To read the header of files
#include "PCDio.h"
#include "LASio.h"

// STL
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

// To parse JSON VPC
#include <nlohmann/json.hpp>
#include <fstream>

#include <ctime>

// Define macros for cross-platform compatibility
#ifdef _WIN32
#define timegm _mkgmtime
inline void gmtime_r(const time_t* timep, std::tm* result)
{
  gmtime_s(result, timep);
}
#endif


bool FileCollection::read(const std::vector<std::string>& files, bool progress)
{
  if (files.size() == 0)
  {
    last_error = "There is no file to read";
    return false;
  }

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
      if (!add_las_file(file)) return false;
    }
    else if (type == PathType::PCDFILE)
    {
      if (!add_pcd_file(file)) return false;
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
          std::string f = entry.path().string();
          PathType type = parse_path(f);
          bool success = true;

          if (type == LASFILE)
            success = add_las_file(f);
          else if (type == PCDFILE)
            success = add_pcd_file(f);

          if (!success) return false;
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

  // Fix #160 with empty folders
  if (this->files.size() == 0)
  {
    last_error = "There is no file to read";
    return false;
  }

  // Check if all headers have the same signature
  const std::string& referenceSignature = headers[0].signature; // Take the CRS of the first header
  bool allSameSignature = std::all_of(headers.begin(), headers.end(), [&referenceSignature](const Header& h) { return h.signature == referenceSignature; });
  if (!allSameSignature)
  {
    last_error = "Impossible to mix different file formats";
    return false;
  }

  // Check if all headers have the same CRS
  const CRS& referenceCRS = headers[0].crs; // Take the CRS of the first header
  bool allSameCRS = std::all_of(headers.begin(), headers.end(), [&referenceCRS](const Header& h) { return h.crs == referenceCRS; });
  if (!allSameCRS) warning("mix CRS found. First one retained\n");
  crs = referenceCRS;

  return true;
}

bool FileCollection::read_vpc(const std::string& filename)
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
        if (!add_las_file(file_path))
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

      Header h;
      h.min_x = min_x;
      h.min_y = min_y;
      h.max_x = max_x;
      h.max_y = max_y;
      h.number_of_point_records = pccount;
      h.spatial_index = spatial_index;
      h.signature = "LASF";
      if (epsg != 0) h.crs = CRS(epsg);
      if (!wkt.empty()) h.crs = CRS(wkt);

      headers.push_back(h);
      files.push_back(file_path);
      noprocess.push_back(false);
      file_index.add(h.min_x, h.min_y, h.max_x, h.max_y);

      if (xmin > h.min_x) xmin = h.min_x;
      if (ymin > h.min_y) ymin = h.min_y;
      if (xmax < h.max_x) xmax = h.max_x;
      if (ymax < h.max_y) ymax = h.max_y;
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

bool FileCollection::write_vpc(const std::string& vpcfile, const CRS& crs, bool absolute_path, bool use_gpstime)
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

    const Header& h = headers[i];

    Rectangle bbox = {h.min_x, h.min_y, h.max_x, h.max_y};
    uint64_t n = h.number_of_point_records;
    bool index = h.spatial_index;
    double zmin = h.min_z;
    double zmax = h.max_z;

    std::string date;
    int year = h.file_creation_year;
    int doy = h.file_creation_day;

    if (use_gpstime)
    {
      if (h.adjusted_standard_gps_time)
      {
        if (h.gpstime == 0)
        {
          warning("The GPS time of the first point is 0. Cannot use GPS time to assign a date.");
        }
        else
        {
          auto date = h.gpstime_date();
          year = date.first;
          doy = date.second;
        }
      }
      else
      {
        warning("The GPS time is not recorded as Adjusted Standard GPS Time but as GPS Week Time. Cannot use GPS time to assign a date.");
      }
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
    snprintf(buffer, sizeof(buffer), "[ [%.9lf,%.9lf], [%.9lf,%.9lf], [%.9lf,%.9lf], [%.9lf,%.9lf], [%.9lf,%.9lf] ]", A.x, A.y, B.x, B.y, C.x, C.y, D.x, D.y, A.x, A.y);
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

void FileCollection::add_query(double xmin, double ymin, double xmax, double ymax)
{
  Rectangle* rect = new Rectangle(xmin, ymin, xmax, ymax);
  queries.push_back(rect);
}

void FileCollection::add_query(double xcenter, double ycenter, double radius)
{
  Circle* circ = new Circle(xcenter, ycenter, radius);
  queries.push_back(circ);
}

bool FileCollection::add_las_file(std::string file, bool noprocess)
{
  std::replace(file.begin(), file.end(), '\\', '/' );

  Header header;
  LASio reader;

  try
  {
    reader.open(file);
    reader.populate_header(&header, true);
    reader.close();
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  if (header.number_of_point_records == 0)
  {
    warning("File %s containing 0 point was discarded.\n", file.c_str());
    return true;
  }

  add_header(header, noprocess);
  files.push_back(file);

  use_dataframe = false;
  return true;
}

bool FileCollection::add_pcd_file(std::string file, bool noprocess)
{
  std::replace(file.begin(), file.end(), '\\', '/' );

  Header header;
  PCDio reader;
  reader.preread_bbox = true;

  try
  {
    reader.open(file);
    reader.populate_header(&header);
    reader.close();
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  add_header(header, noprocess);
  files.push_back(file);

  use_dataframe = false;
  return true;
}


#ifdef USING_R
// Special to build a FileCollection from a data.frame in R
void FileCollection::add_dataframe(double xmin, double ymin, double xmax, double ymax, int npoints)
{
  Header h;
  h.min_x = xmin;
  h.min_y = ymin;
  h.max_x = xmax;
  h.max_y = ymax;
  h.number_of_point_records = npoints;
  h.signature = "data.frame";

  add_header(h);
  files.push_back("data.frame");

  use_dataframe = true;
}

void FileCollection::add_xptr(Header header)
{
  header.signature = "xptr";
  add_header(header);
  files.push_back("xptr");

  use_dataframe = false;
}
#endif

bool FileCollection::add_header(const Header& header, bool noprocess)
{
  headers.push_back(header);

  if (xmin > header.min_x) xmin = header.min_x;
  if (ymin > header.min_y) ymin = header.min_y;
  if (xmax < header.max_x) xmax = header.max_x;
  if (ymax < header.max_y) ymax = header.max_y;

  this->noprocess.push_back(noprocess);
  this->file_index.add(header.min_x, header.min_y, header.max_x, header.max_y);

  return true;
}

bool FileCollection::set_noprocess(const std::vector<bool>& b)
{
  if (b.size() != files.size())
  {
    last_error = "the vector indicating if a file is used as buffer only is not the same size as the vector of files"; // # nocov
    return false; // # nocov
  }

  noprocess = b;
  return true;
}

bool FileCollection::set_chunk_size(double size)
{
  chunk_size = 0;

  if (size > 0)
  {
    if (queries.size() > 0)
    {
      last_error = "Impossible to set chunk size with queries";
      return false;
    }

    chunk_size = size;

    Grid grid(xmin, ymin, xmax, ymax, chunk_size);
    for (int i = 0 ; i < grid.get_ncells() ; i++)
    {
      double x = grid.x_from_cell(i);
      double y = grid.y_from_cell(i);
      double hsize = size/2;

      if (file_index.has_overlap(x-hsize, y-hsize, x+hsize, y+hsize))
        add_query(x-hsize, y-hsize, x+hsize, y+hsize);
    }
  }

  return true;
}

bool FileCollection::get_chunk(int i, Chunk& chunk) const
{
  if (i < 0 || i > get_number_chunks())
  {
    last_error = "chunk request out of bounds"; // # nocov
    return false; // # nocov
  }

  bool success = false;

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

  chunk.id = i;

  return success;
}

const std::vector<std::filesystem::path>& FileCollection::get_files() const
{
  return files;
}

PathType FileCollection::get_format() const
{
  const std::string& signature = headers[0].signature;

  if (signature == "LASF")
    return LASFILE;
  else if (signature == "PCDF")
    return PCDFILE;
  else if (signature == "data.frame")
    return DATAFRAME;
  else if (signature == "xptr")
    return XPTR;
  else
    return UNKNOWNFILE;
}

bool FileCollection::get_chunk_regular(int i, Chunk& chunk) const
{
  chunk.clear();

  const Header& h = headers[i];

  chunk.xmin = h.min_x;
  chunk.ymin = h.min_y;
  chunk.xmax = h.max_x;
  chunk.ymax = h.max_y;

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
  std::vector<int> indexes = file_index.get_overlaps(h.min_x - buffer, h.min_y - buffer, h.max_x + buffer, h.max_y + buffer);
  for (auto index : indexes)
  {
    std::string file = files[index].string();
    if (chunk.main_files[0] != file)
    {
      chunk.neighbour_files.push_back(file);
    }
  }

  return true;
}

bool FileCollection::get_chunk_with_query(int i, Chunk& chunk) const
{
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
  std::vector<int> indexes = file_index.get_overlaps(minx - buffer, miny - buffer,  maxx + buffer, maxy + buffer);
  if (indexes.empty())
  {
    // There is no match, create a placeholder that won't be read
    chunk.clear();
    char buff[128];
    snprintf(buff, sizeof(buff), "cannot find any file in [%.1lf, %.1lf, %.1lf, %.1lf]\n", minx, miny,  maxx, maxy);
    warning(buff);
    return true;
  }
  else
  {
    // There is a match we can update the chunk
    chunk.xmin = minx;
    chunk.ymin = miny;
    chunk.xmax = maxx;
    chunk.ymax = maxy;
  }

  if (chunk.xmin < xmin) chunk.xmin = xmin;
  if (chunk.xmax > xmax) chunk.xmax = xmax;
  if (chunk.ymin < ymin) chunk.ymin = ymin;
  if (chunk.ymax > ymax) chunk.ymax = ymax;
  chunk.buffer = buffer;
  chunk.shape = q->type();

  // With an R data.frame there is no file and thus no neighboring files. We can exit.
  if (use_dataframe)
  {
    chunk.main_files.push_back("dataframe");
    chunk.name = "data.frame";
    return true;
  }

  // We are working with a single file there is no neighboring files. We can exit.
  if (get_number_files() == 1)
  {
    chunk.main_files.push_back(files[0].string());
    chunk.name = files[0].stem().string() + "_" + std::to_string(i);
    return true;
  }

  // There are likely multiple files that intersect the query. If there is only one it is easy. We can exit.
  if (indexes.size() == 1)
  {
    int index = indexes[0];
    chunk.main_files.push_back(files[index].string());
    chunk.name = files[index].stem().string() + "_" + std::to_string(i);
    return true;
  }

  // There are multiple files. All the files are part of the main
  for (auto index : indexes)
  {
    chunk.main_files.push_back(files[index].string());
    if (chunk.name.empty()) chunk.name = files[index].stem().string() + "_" + std::to_string(i);
  }

  // We search the file that contains the centroid of the query to assign a name to the query
  // If we can't find it we have already assigned a name anyway.
  indexes = file_index.get_overlaps(centerx - epsilon, centery - epsilon,  centerx + epsilon, centery + epsilon);
  if (!indexes.empty())
  {
    int index = indexes[0];
    chunk.name = files[index].stem().string() + "_" + std::to_string(i);
  }

  // We perform a query again with buffered shape to get the other files in the buffer
  if (chunk.buffer > 0)
  {
    indexes = file_index.get_overlaps(minx - buffer, miny - buffer, maxx + buffer, maxy + buffer);
    for (auto index : indexes)
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

bool FileCollection::check_spatial_index()
{
  bool multi_files = get_number_files() > 1;
  bool use_buffer = get_buffer() > 0;
  bool no_index = get_number_indexed_files() != get_number_files();
  bool has_queries = queries.size() > 1;
  return !((multi_files && use_buffer && no_index) || (has_queries && no_index));
}

int FileCollection::get_number_chunks() const
{
  return (queries.size() == 0) ? get_number_files() : queries.size();
}

int FileCollection::get_number_files() const
{
  return files.size();
}

int FileCollection::get_number_indexed_files() const
{
  int count = std::count_if(headers.begin(), headers.end(), [](const Header& h) { return h.spatial_index; });
  return count;
}

void FileCollection::set_all_indexed()
{
  for (Header& h : headers) h.spatial_index = true;
}

void FileCollection::clear()
{
  xmin = std::numeric_limits<double>::max();
  ymin = std::numeric_limits<double>::max();
  xmax = -std::numeric_limits<double>::max();
  ymax = -std::numeric_limits<double>::max();
  last_error.clear();

  use_dataframe = true;
  use_vpc = false;

  buffer = 0;
  chunk_size = 0;

  headers.clear();
  noprocess.clear();
  files.clear();

  for (auto p : queries) delete p;
  queries.clear();
}

bool FileCollection::file_exists(std::string& file)
{
  auto it = std::find(files.begin(), files.end(), file);
  return it != files.end();
}

PathType FileCollection::parse_path(const std::string& path)
{
  std::filesystem::path file_path(path);

  if (std::filesystem::exists(file_path))
  {
    if (std::filesystem::is_regular_file(file_path))
    {
      std::string ext = file_path.extension().string();
      if (ext == ".vpc" || ext == ".vpc") return PathType::VPCFILE;
      if (ext == ".las" || ext == ".LAS") return PathType::LASFILE;
      if (ext == ".laz" || ext == ".LAZ") return PathType::LASFILE;
      if (ext == ".lax" || ext == ".LAX") return PathType::LAXFILE;
      if (ext == ".pcd" || ext == ".PCD") return PathType::PCDFILE;
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


FileCollection::FileCollection()
{
  clear();
}

FileCollection::~FileCollection()
{
  for (auto p : queries) delete p;
}

void FileCollectionIndex::add(double xmin, double ymin, double xmax, double ymax)
{
  bboxes.emplace_back(xmin, ymin, xmax, ymax);
}

bool FileCollectionIndex::has_overlap(double xmin, double ymin, double xmax, double ymax) const
{
  for (const auto& bbox : bboxes)
  {
    if (!(xmax < bbox.xmin() || xmin > bbox.xmax() || ymax < bbox.ymin() || ymin > bbox.ymax()))
    {
      return true;
    }
  }
  return false;
}

std::vector<int> FileCollectionIndex::get_overlaps(double xmin, double ymin, double xmax, double ymax) const
{
  std::vector<int> overlaps;
  for (size_t i = 0; i < bboxes.size(); ++i)
  {
    const auto& bbox = bboxes[i];

    if (xmin <= bbox.xmax() && xmax >= bbox.xmin() && ymin <= bbox.ymax() && ymax >= bbox.ymin())
    {
      overlaps.push_back(i);
    }
  }

  return overlaps;
}

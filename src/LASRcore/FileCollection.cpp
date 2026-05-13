// LASR
#include "FileCollection.h"
#include "Progress.h"
#include "error.h"
#include "print.h"
#include "Grid.h"
#include "PointSchema.h"

#ifdef USING_GDAL
#include <cpl_conv.h>
#endif

// To read the header of files
#include "PCDio.h"
#include "LASio.h"
#include "EPTio.h"

// STL
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <unordered_set>

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

static bool is_remote_path(const std::string& path)
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

static std::string filename_from_url(const std::string& url)
{
  // Strip query parameters
  std::string path = url.substr(0, url.find('?'));
  // Find the last slash
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) return path;
  std::string name = path.substr(pos + 1);
  // Strip .laz or .las extension, and .copc suffix if present
  size_t dot = name.rfind('.');
  if (dot != std::string::npos) name = name.substr(0, dot);
  if (name.size() > 5 && name.substr(name.size() - 5) == ".copc")
    name = name.substr(0, name.size() - 5);
  return name;
}

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

    // A LAS, LAZ, or remote LAS/LAZ file
    if (type == PathType::LASFILE || type == PathType::REMOTELASFILE)
    {
      if (!add_las_file(file)) return false;
    }
    else if (type == PathType::EPTFILE || type == PathType::REMOTEEPTFILE)
    {
      if (!add_ept_endpoint(file)) return false;
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

    // Track whether every feature provides a 3D bbox. If some do and
    // some don't, the catalog's accumulated z-bounds would only cover
    // a subset of files while point counts cover all of them — this
    // produces a wrong octree size on merged COPC writes. Reject the
    // mix loudly instead of silently producing skewed output.
    int n_features_with_z = 0;
    int n_features_total = 0;

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
      if (!is_remote_path(file_path))
      {
        file_path = std::filesystem::weakly_canonical(parent_folder / file_path).string();
      }

      // pc:count via 64-bit. STAC's pointcloud extension says the field is
      // an integer with no upper bound; reading as int silently truncates
      // catalogs over ~2.1B points, which would mis-size the merged COPC
      // octree's auto max_depth.
      uint64_t pccount = feature["properties"]["pc:count"].get<uint64_t>();

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

      // We are sure there is a bbox. STAC proj:bbox is [minx,miny,minz,maxx,maxy,maxz]
      // for 3D and [minx,miny,maxx,maxy] for 2D. Read z when available so the
      // FileCollection's union z-bounds are tight; merged COPC outputs need all
      // three axes to size the octree correctly.
      auto bbox = feature["properties"]["proj:bbox"];
      double min_x, min_y, max_x, max_y;
      double min_z = 0.0, max_z = 0.0;
      bool have_z = false;
      if (bbox.size() == 6)
      {
        min_x = bbox[0].get<double>();
        min_y = bbox[1].get<double>();
        min_z = bbox[2].get<double>();
        max_x = bbox[3].get<double>();
        max_y = bbox[4].get<double>();
        max_z = bbox[5].get<double>();
        have_z = true;
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
      if (have_z)
      {
        h.min_z = min_z;
        h.max_z = max_z;
      }
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
      if (have_z)
      {
        if (zmin > h.min_z) zmin = h.min_z;
        if (zmax < h.max_z) zmax = h.max_z;
        n_features_with_z++;
      }
      n_features_total++;
    }

    if (n_features_with_z != 0 && n_features_with_z != n_features_total)
    {
      last_error = "Malformed virtual point cloud file: " +
        std::to_string(n_features_with_z) + " of " +
        std::to_string(n_features_total) + " features carry a 3D proj:bbox; "
        "the rest are 2D. Mixed 4D/6D bboxes produce a partial z range over "
        "only some inputs while point counts cover all of them, which sizes "
        "the merged COPC octree incorrectly. Make every feature's "
        "properties.proj:bbox 6-element [minx, miny, minz, maxx, maxy, maxz].";
      return false;
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
    // STAC's top-level "bbox" lives in WGS 84 (EPSG:4979). Transform a
    // mutable copy of the source z so the source-CRS z values stay
    // available for properties.proj:bbox below — feeding the transformed
    // values into proj:bbox while x/y are kept in source coords would
    // make proj:bbox internally inconsistent if the vertical transform
    // shifts z.
    double top_zmin = zmin;
    double top_zmax = zmax;
    double z_throwaway = 0;
    if (!poTransform->Transform(1, &A.x, &A.y, &top_zmin) ||
        !poTransform->Transform(1, &B.x, &B.y, &z_throwaway) ||
        !poTransform->Transform(1, &C.x, &C.y, &top_zmax) ||
        !poTransform->Transform(1, &D.x, &D.y, &z_throwaway))
    {
      last_error = "Transformation of the bounding in WGS 84 failed!";
      return false;
    }

    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "[ [%.9lf,%.9lf], [%.9lf,%.9lf], [%.9lf,%.9lf], [%.9lf,%.9lf], [%.9lf,%.9lf] ]", A.x, A.y, B.x, B.y, C.x, C.y, D.x, D.y, A.x, A.y);
    std::string geometry(buffer);
    snprintf(buffer, sizeof(buffer), "[%.9lf, %.9lf, %.3lf, %.9lf, %.9lf, %.3lf]", MIN(A.x, D.x), MIN(A.y, B.y), top_zmin, MAX(B.x, C.x), MAX(C.y, D.y), top_zmax);
    std::string sbbox(buffer);

    std::string id = file.stem().string();
    if (file.extension() == ".laz" && id.size() > 5 && id.substr(id.size() - 5) == ".copc") {
      id = id.substr(0, id.size() - 5);
    }

    output << "  {" << std::endl;
    output << "    \"type\": \"Feature\"," << std::endl;
    output << "    \"stac_version\": \"1.0.0\"," << std::endl;
    output << "    \"stac_extensions\": [" << std::endl;
    output << "      \"https://stac-extensions.github.io/pointcloud/v1.0.0/schema.json\"," << std::endl;
    output << "      \"https://stac-extensions.github.io/projection/v1.1.0/schema.json\"" << std::endl;
    output << "     ]," << std::endl;
    output << "    \"id\": " << autoquote(id) << "," << std::endl;
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
    // 6-element STAC proj:bbox = [minx, miny, minz, maxx, maxy, maxz]. The
    // older 4-element form is allowed by the spec but loses z, and lasR's
    // own VPC reader (read_vpc) only inspects properties.proj:bbox — not
    // the top-level "bbox" field — when populating the FileCollection's
    // 3D bounds. A 4D properties.proj:bbox round-tripped into a merged
    // COPC write produces no z bounds, which then disables the union
    // bbox/count override on the writer side and falls back to file-1's
    // bbox alone.
    output << "      \"proj:bbox\": ["
           << std::fixed << std::setprecision(3)
           << bbox.minx << ", " << bbox.miny << ", " << zmin << ", "
           << bbox.maxx << ", " << bbox.maxy << ", " << zmax
           << "]," << std::endl;
    if (!wkt.empty()) output << "      \"proj:wkt2\": " << wkt;
    if (epsg != 0) output << "," << std::endl << "      \"proj:epsg\": " << epsg << std::endl;
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
  add_query(xmin, ymin, xmax, ymax, false);
}

void FileCollection::add_query(double xmin, double ymin, double xmax, double ymax, bool strict_clip)
{
  add_query(xmin, ymin, xmax, ymax, strict_clip, std::nan(""), std::nan(""));
}

void FileCollection::add_query(double xmin, double ymin, double xmax, double ymax,
                               bool strict_clip,
                               double owner_xmax, double owner_ymax)
{
  QueryRecord q;
  q.shape       = std::unique_ptr<Shape>(new Rectangle(xmin, ymin, xmax, ymax));
  q.strict_clip = strict_clip;
  q.owner_xmax  = owner_xmax;
  q.owner_ymax  = owner_ymax;
  queries.push_back(std::move(q));
}

void FileCollection::add_query(double xcenter, double ycenter, double radius)
{
  add_query(xcenter, ycenter, radius, false);
}

void FileCollection::add_query(double xcenter, double ycenter, double radius, bool strict_clip)
{
  QueryRecord q;
  q.shape       = std::unique_ptr<Shape>(new Circle(xcenter, ycenter, radius));
  q.strict_clip = strict_clip;
  // owner_xmax / owner_ymax default to NaN; circles never carry an
  // owner override (partition_ept passes circles through unchanged).
  queries.push_back(std::move(q));
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

bool FileCollection::add_ept_endpoint(std::string path, bool noprocess)
{
  if (files.size() > 0) {
    last_error = "Only a single EPT endpoint is supported";
    return false;
  }
  std::replace(path.begin(), path.end(), '\\', '/');

  // Tune GDAL/VSI defaults for remote EPT reads. Without these, every
  // /vsicurl/ open does sibling-directory HEAD probing (≈10× more HTTP
  // requests than the actual range fetch), no chunk caching means
  // overlapping AOI sub-queries re-fetch the same bytes, and HTTP/1.1
  // serializes per host. Measured impact on autzen-classified
  // concurrent_files(4): warm-cache reads 42 s → 0.9 s, cold-cache
  // reads 43 s → 13 s.
  //
  // CPLSetConfigOption falls through to env vars on read, so any
  // user override (e.g. Sys.setenv) keeps precedence — only set when
  // unset. Scope: process-wide for all subsequent VSI ops, fine for
  // lasR's typical usage.
#ifdef USING_GDAL
  if (CPLGetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", nullptr) == nullptr)
    CPLSetConfigOption("GDAL_DISABLE_READDIR_ON_OPEN", "EMPTY_DIR");
  if (CPLGetConfigOption("GDAL_HTTP_MULTIPLEX", nullptr) == nullptr)
    CPLSetConfigOption("GDAL_HTTP_MULTIPLEX", "YES");
  if (CPLGetConfigOption("VSI_CACHE", nullptr) == nullptr)
    CPLSetConfigOption("VSI_CACHE", "TRUE");
  if (CPLGetConfigOption("VSI_CACHE_SIZE", nullptr) == nullptr)
    CPLSetConfigOption("VSI_CACHE_SIZE", "67108864");  // 64 MiB
#endif

  try {
    ept_index = EPTio::HierarchyIndex::build_metadata(path);
  } catch (const std::exception& e) {
    last_error = e.what();
    return false;
  }

  Header header;
  try {
    LASio probe;
    probe.open(ept_index->probe_tile_path);
    probe.populate_header(&header);
    probe.close();
  } catch (const std::exception& e) {
    last_error = e.what();
    return false;
  }

  header.signature = "EPTF";
  header.min_x = ept_index->conf_bounds[0];
  header.min_y = ept_index->conf_bounds[1];
  header.min_z = ept_index->conf_bounds[2];
  header.max_x = ept_index->conf_bounds[3];
  header.max_y = ept_index->conf_bounds[4];
  header.max_z = ept_index->conf_bounds[5];
  if (!ept_index->srs_wkt.empty()) header.set_crs(ept_index->srs_wkt);
  else if (ept_index->srs_epsg > 0) header.set_crs(ept_index->srs_epsg);
  header.spatial_index = true;

  add_header(header, noprocess);
  files.push_back(path);
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
  if (zmin > header.min_z) zmin = header.min_z;
  if (zmax < header.max_z) zmax = header.max_z;

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
  return set_chunk_size(size, false);
}

bool FileCollection::set_chunk_size(double size, bool strict_clip)
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
        add_query(x-hsize, y-hsize, x+hsize, y+hsize, strict_clip);
    }
  }

  return true;
}

// Deep-copies a Shape* by dispatching on its type. Used by the
// exception-safe rebuild in partition_ept. Returns unique_ptr so the
// clone is freed if any push_back throws bad_alloc at the call site.
static std::unique_ptr<Shape> clone_shape(const Shape* s)
{
  switch (s->type()) {
    case ShapeType::RECTANGLE:
      return std::unique_ptr<Shape>(
          new Rectangle(s->xmin(), s->ymin(), s->xmax(), s->ymax()));
    case ShapeType::CIRCLE: {
      const Circle* c = static_cast<const Circle*>(s);
      return std::unique_ptr<Shape>(new Circle(c->center.x, c->center.y, c->radius));
    }
    default:
      throw std::runtime_error("clone_shape: unsupported Shape type");
  }
}

int FileCollection::ept_pick_depth_for_aois(
    double cube_xmin, double cube_ymin, double cube_size,
    int max_tile_depth, int target_partitions,
    const std::vector<std::array<double, 4>>& aoi_bboxes)
{
  // Iterate octree depth `d` upward; at each `d` count the integer
  // cells covered by the union of AOI bboxes. Stop at the smallest
  // `d` whose total cell count meets target_partitions, capped at
  // min(16, max_tile_depth). Monotone in `d` so at most 17 steps.
  //
  // The previous heuristic used an area ratio
  //   d = ceil(0.5 * log2(target * cube_area / aoi_area))
  // which was correct for square AOIs but undercounted long-thin
  // ones: a 280 m × 5 m strip in a 285 m cube has tiny area but
  // already spans the cube in x.
  const int cap = (max_tile_depth < 16 ? max_tile_depth : 16);
  if (cap <= 0 || target_partitions <= 0) return 0;

  auto cells_at = [&](int dd) -> int64_t {
    const double cell = cube_size / (double)(int64_t{1} << dd);
    int64_t total = 0;
    for (const auto& a : aoi_bboxes) {
      int64_t cx_lo = (int64_t)std::floor((a[0] - cube_xmin) / cell);
      int64_t cx_hi = (int64_t)std::ceil ((a[2] - cube_xmin) / cell);
      int64_t cy_lo = (int64_t)std::floor((a[1] - cube_ymin) / cell);
      int64_t cy_hi = (int64_t)std::ceil ((a[3] - cube_ymin) / cell);
      total += (cx_hi - cx_lo) * (cy_hi - cy_lo);
    }
    return total;
  };

  int d = 0;
  while (d < cap && cells_at(d) < (int64_t)target_partitions) d++;
  return d;
}

bool FileCollection::partition_ept(int target_partitions)
{
  if (!ept_index || get_format() != EPTFILE) return true;
  if (chunk_size > 0) return true;
  if (target_partitions <= 0) return true;

  const bool had_queries = !queries.empty();

  // Pre-extract AOI rectangles so the hierarchy walk can prune subtrees that
  // sit entirely outside the AOI. Without this, a continent-scale EPT (e.g.
  // 80B-point USGS Sierra Nevada) does a full breadth-first hierarchy
  // enumeration before partitioning, which dominates startup. The same
  // bboxes are recomputed inside the with-queries path below for the
  // AoiBbox struct used by partition logic — kept local to avoid churning
  // that block.
  std::vector<std::array<double, 4>> walk_aoi_bboxes;
  if (had_queries) {
    const double cxmin0 = ept_index->conf_bounds[0];
    const double cymin0 = ept_index->conf_bounds[1];
    const double cxmax0 = ept_index->conf_bounds[3];
    const double cymax0 = ept_index->conf_bounds[4];
    walk_aoi_bboxes.reserve(queries.size());
    for (const auto& qr : queries) {
      const Shape* q = qr.shape.get();
      if (q->type() != ShapeType::RECTANGLE) continue;
      const double bxmin = std::max(q->xmin(), cxmin0);
      const double bymin = std::max(q->ymin(), cymin0);
      const double bxmax = std::min(q->xmax(), cxmax0);
      const double bymax = std::min(q->ymax(), cymax0);
      if (bxmin >= bxmax || bymin >= bymax) continue;
      walk_aoi_bboxes.push_back({bxmin, bymin, bxmax, bymax});
    }
  }

  // Hierarchy walk; with-queries fallback diverges from no-queries fallback.
  // The AOI filter is empty for the no-queries path (full walk, cacheable).
  try { ept_index->ensure_tiles(walk_aoi_bboxes); }
  catch (const std::exception& e) {
    if (had_queries) {
      warning("EPT hierarchy unavailable (%s); AOI partitioning skipped\n", e.what());
      return true;
    }
    warning("EPT hierarchy walk failed (%s); falling back to grid chunking\n", e.what());
    double dx = ept_index->conf_bounds[3] - ept_index->conf_bounds[0];
    double dy = ept_index->conf_bounds[4] - ept_index->conf_bounds[1];
    double size = std::sqrt((dx * dy) / (double)target_partitions);
    return set_chunk_size(size, true);
  }

  if (ept_index->tiles.empty()) {
    if (had_queries) {
      warning("EPT hierarchy yielded no tiles; AOI partitioning skipped\n");
      return true;
    }
    warning("EPT hierarchy yielded no tiles; falling back to grid chunking\n");
    double dx = ept_index->conf_bounds[3] - ept_index->conf_bounds[0];
    double dy = ept_index->conf_bounds[4] - ept_index->conf_bounds[1];
    double size = std::sqrt((dx * dy) / (double)target_partitions);
    return set_chunk_size(size, true);
  }

  // ---- No-queries path: today's behavior, byte-for-byte ----
  if (!had_queries) {
    // Cap d at 16 (EPT hard depth limit) before evaluating the shift
    // so huge target_partitions values cannot trigger UB.
    int d = 0;
    while (d < 16 && (1 << (2 * d)) < target_partitions) d++;
    int max_tile_depth = 0;
    for (const auto& t : ept_index->tiles)
      if (t.key.d > max_tile_depth) max_tile_depth = t.key.d;
    if (d > max_tile_depth) d = max_tile_depth;

    const double cube_size = ept_index->cube_bounds[3] - ept_index->cube_bounds[0];
    const double cell_size = cube_size / (double)(1 << d);
    const int grid_n = 1 << d;

    std::vector<int64_t> cell_points((size_t)grid_n * grid_n, 0);
    for (const auto& t : ept_index->tiles) {
      if (t.key.d >= d) {
        int shift = t.key.d - d;
        int cx = t.key.x >> shift;
        int cy = t.key.y >> shift;
        if (cx < 0 || cx >= grid_n || cy < 0 || cy >= grid_n) continue;
        cell_points[(size_t)cy * grid_n + cx] += t.point_count;
      } else {
        int span = 1 << (d - t.key.d);
        int cx0 = t.key.x * span;
        int cy0 = t.key.y * span;
        for (int iy = 0; iy < span; ++iy)
          for (int ix = 0; ix < span; ++ix) {
            int cx = cx0 + ix;
            int cy = cy0 + iy;
            if (cx < 0 || cx >= grid_n || cy < 0 || cy >= grid_n) continue;
            cell_points[(size_t)cy * grid_n + cx] += t.point_count;
          }
      }
    }

    const double cxmin = ept_index->conf_bounds[0];
    const double cymin = ept_index->conf_bounds[1];
    const double cxmax = ept_index->conf_bounds[3];
    const double cymax = ept_index->conf_bounds[4];

    int emitted = 0;
    for (int cy = 0; cy < grid_n; ++cy) {
      for (int cx = 0; cx < grid_n; ++cx) {
        if (cell_points[(size_t)cy * grid_n + cx] == 0) continue;
        double cxmin_c = ept_index->cube_bounds[0] + cx * cell_size;
        double cymin_c = ept_index->cube_bounds[1] + cy * cell_size;
        double cxmax_c = cxmin_c + cell_size;
        double cymax_c = cymin_c + cell_size;
        if (cxmin_c < cxmin) cxmin_c = cxmin;
        if (cymin_c < cymin) cymin_c = cymin;
        if (cxmax_c > cxmax) cxmax_c = cxmax;
        if (cymax_c > cymax) cymax_c = cymax;
        if (cxmin_c >= cxmax_c || cymin_c >= cymax_c) continue;
        add_query(cxmin_c, cymin_c, cxmax_c, cymax_c, true);
        emitted++;
      }
    }
    if (emitted == 0) {
      warning("EPT partitioning produced no chunks after conforming clamp; falling back to grid chunking\n");
      double dx = ept_index->conf_bounds[3] - ept_index->conf_bounds[0];
      double dy = ept_index->conf_bounds[4] - ept_index->conf_bounds[1];
      double size = std::sqrt((dx * dy) / (double)target_partitions);
      return set_chunk_size(size, true);
    }
    return true;
  }

  // ---- With-queries path ----

  // Compute clamped AOI bbox per rect query and aggregate area.
  struct AoiBbox { double xmin, ymin, xmax, ymax; bool partitionable; };
  std::vector<AoiBbox> aoi_bboxes(queries.size(),
      AoiBbox{0, 0, 0, 0, false});

  const double cxmin = ept_index->conf_bounds[0];
  const double cymin = ept_index->conf_bounds[1];
  const double cxmax = ept_index->conf_bounds[3];
  const double cymax = ept_index->conf_bounds[4];

  bool any_partitionable = false;
  for (size_t i = 0; i < queries.size(); ++i) {
    const Shape* q = queries[i].shape.get();
    if (q->type() != ShapeType::RECTANGLE) continue;
    double bxmin = std::max(q->xmin(), cxmin);
    double bymin = std::max(q->ymin(), cymin);
    double bxmax = std::min(q->xmax(), cxmax);
    double bymax = std::min(q->ymax(), cymax);
    if (bxmin >= bxmax || bymin >= bymax) continue;  // disjoint → passthrough
    aoi_bboxes[i] = {bxmin, bymin, bxmax, bymax, true};
    any_partitionable = true;
  }

  if (!any_partitionable) return true;  // nothing partitionable; leave queries unchanged

  // Depth selection — see ept_pick_depth_for_aois for the rationale.
  // The previous area-ratio heuristic undercounted long-thin AOIs;
  // the cell-count iteration is what we want.
  const double cube_size = ept_index->cube_bounds[3] - ept_index->cube_bounds[0];
  int max_tile_depth = 0;
  for (const auto& t : ept_index->tiles)
    if (t.key.d > max_tile_depth) max_tile_depth = t.key.d;

  std::vector<std::array<double, 4>> aoi_arrays;
  aoi_arrays.reserve(aoi_bboxes.size());
  for (const auto& a : aoi_bboxes) {
    if (!a.partitionable) continue;
    aoi_arrays.push_back({a.xmin, a.ymin, a.xmax, a.ymax});
  }
  const int d = ept_pick_depth_for_aois(
      ept_index->cube_bounds[0], ept_index->cube_bounds[1], cube_size,
      max_tile_depth, target_partitions, aoi_arrays);

  const double cell_size = cube_size / (double)(1 << d);
  const int grid_n = 1 << d;

  // Per-AOI cell range and union range. The cell-occupancy map is
  // restricted to the union — memory and walk time scale with the AOI
  // footprint, not with grid_n^2. (A naive 2^d × 2^d int64_t map would
  // be ~32 GiB at d=16; here it's bounded by the union of AOI cell
  // ranges, which is at most O(target_partitions) cells for a
  // well-chosen depth.)
  struct AoiCellRange { int cx_lo, cx_hi, cy_lo, cy_hi; };
  std::vector<AoiCellRange> aoi_ranges(queries.size(),
      AoiCellRange{0, 0, 0, 0});
  int u_cx_lo = grid_n, u_cy_lo = grid_n;
  int u_cx_hi = 0,      u_cy_hi = 0;
  for (size_t i = 0; i < queries.size(); ++i) {
    if (!aoi_bboxes[i].partitionable) continue;
    const AoiBbox& a = aoi_bboxes[i];
    int cx_lo = (int)std::floor((a.xmin - ept_index->cube_bounds[0]) / cell_size);
    int cx_hi = (int)std::ceil ((a.xmax - ept_index->cube_bounds[0]) / cell_size);
    int cy_lo = (int)std::floor((a.ymin - ept_index->cube_bounds[1]) / cell_size);
    int cy_hi = (int)std::ceil ((a.ymax - ept_index->cube_bounds[1]) / cell_size);
    if (cx_lo < 0) cx_lo = 0;
    if (cy_lo < 0) cy_lo = 0;
    if (cx_hi > grid_n) cx_hi = grid_n;
    if (cy_hi > grid_n) cy_hi = grid_n;
    aoi_ranges[i] = {cx_lo, cx_hi, cy_lo, cy_hi};
    if (cx_lo < u_cx_lo) u_cx_lo = cx_lo;
    if (cy_lo < u_cy_lo) u_cy_lo = cy_lo;
    if (cx_hi > u_cx_hi) u_cx_hi = cx_hi;
    if (cy_hi > u_cy_hi) u_cy_hi = cy_hi;
  }

  // Sparse occupancy keyed by cy * grid_n + cx. Storing the cell as a
  // single uint64 keeps memory proportional to the number of occupied
  // cells inside the AOI union, never to grid_n^2.
  const uint64_t G = (uint64_t)grid_n;
  std::unordered_set<uint64_t> occupied;
  for (const auto& t : ept_index->tiles) {
    if (t.key.d >= d) {
      int shift = t.key.d - d;
      int cx = t.key.x >> shift;
      int cy = t.key.y >> shift;
      if (cx < u_cx_lo || cx >= u_cx_hi || cy < u_cy_lo || cy >= u_cy_hi) continue;
      occupied.insert((uint64_t)cy * G + (uint64_t)cx);
    } else {
      int span = 1 << (d - t.key.d);
      int cx0 = t.key.x * span;
      int cy0 = t.key.y * span;
      // Pre-clip the splat range to the AOI union. Without this, a
      // shallow tile at depth 0 in a deep grid would iterate up to
      // 4^d cells per tile.
      int ix0 = std::max(0, u_cx_lo - cx0);
      int iy0 = std::max(0, u_cy_lo - cy0);
      int ix1 = std::min(span, u_cx_hi - cx0);
      int iy1 = std::min(span, u_cy_hi - cy0);
      for (int iy = iy0; iy < iy1; ++iy) {
        for (int ix = ix0; ix < ix1; ++ix) {
          int cx = cx0 + ix;
          int cy = cy0 + iy;
          if (cx < 0 || cx >= grid_n || cy < 0 || cy >= grid_n) continue;
          occupied.insert((uint64_t)cy * G + (uint64_t)cx);
        }
      }
    }
  }

  // Build replacement off-side. Each QueryRecord owns its shape via
  // unique_ptr, so if any push_back throws bad_alloc the in-flight
  // records are freed cleanly and the live queries vector is untouched.
  std::vector<QueryRecord> replacement;

  auto push_passthrough = [&](size_t i) {
    QueryRecord r;
    r.shape       = clone_shape(queries[i].shape.get());
    r.strict_clip = queries[i].strict_clip;
    r.owner_xmax  = queries[i].owner_xmax;
    r.owner_ymax  = queries[i].owner_ymax;
    replacement.push_back(std::move(r));
  };

  for (size_t i = 0; i < queries.size(); ++i) {
    if (!aoi_bboxes[i].partitionable) { push_passthrough(i); continue; }

    const AoiBbox&     a = aoi_bboxes[i];
    const AoiCellRange r = aoi_ranges[i];

    int emitted_for_this = 0;
    for (int cy = r.cy_lo; cy < r.cy_hi; ++cy) {
      for (int cx = r.cx_lo; cx < r.cx_hi; ++cx) {
        if (occupied.count((uint64_t)cy * G + (uint64_t)cx) == 0) continue;
        double cell_xmin = ept_index->cube_bounds[0] + cx * cell_size;
        double cell_ymin = ept_index->cube_bounds[1] + cy * cell_size;
        double cell_xmax = cell_xmin + cell_size;
        double cell_ymax = cell_ymin + cell_size;
        double sxmin = std::max(cell_xmin, a.xmin);
        double symin = std::max(cell_ymin, a.ymin);
        double sxmax = std::min(cell_xmax, a.xmax);
        double symax = std::min(cell_ymax, a.ymax);
        if (sxmin >= sxmax || symin >= symax) continue;
        QueryRecord sub;
        sub.shape       = std::unique_ptr<Shape>(new Rectangle(sxmin, symin, sxmax, symax));
        sub.strict_clip = true;
        sub.owner_xmax  = a.xmax;  // CLAMPED AOI xmax, NOT query.xmax()
        sub.owner_ymax  = a.ymax;
        replacement.push_back(std::move(sub));
        emitted_for_this++;
      }
    }
    if (emitted_for_this == 0) push_passthrough(i);
  }

  // Atomic commit: vector::swap is noexcept; the old QueryRecords (and
  // their unique_ptr shapes) are destroyed when `replacement` exits scope.
  queries.swap(replacement);
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
  else if (signature == "EPTF")
    return EPTFILE;
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

  chunk.catalog_xmax = xmax;
  chunk.catalog_ymax = ymax;

  const Header& h = headers[i];

  chunk.xmin = h.min_x;
  chunk.ymin = h.min_y;
  chunk.xmax = h.max_x;
  chunk.ymax = h.max_y;

  if (!use_dataframe)
  {
    std::string filepath = files[i].string();
    chunk.main_files.push_back(filepath);
    if (is_remote_path(filepath))
      chunk.name = filename_from_url(filepath);
    else
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
  const QueryRecord& rec = queries[i];
  const Shape* q = rec.shape.get();
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
  chunk.catalog_xmax = xmax;
  chunk.catalog_ymax = ymax;
  if (!std::isnan(rec.owner_xmax)) chunk.catalog_xmax = rec.owner_xmax;
  if (!std::isnan(rec.owner_ymax)) chunk.catalog_ymax = rec.owner_ymax;
  chunk.shape = q->type();
  chunk.strict_clip = rec.strict_clip;

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
  zmin = std::numeric_limits<double>::max();
  xmax = -std::numeric_limits<double>::max();
  ymax = -std::numeric_limits<double>::max();
  zmax = -std::numeric_limits<double>::max();
  last_error.clear();

  use_dataframe = true;
  use_vpc = false;

  buffer = 0;
  chunk_size = 0;

  headers.clear();
  noprocess.clear();
  files.clear();

  queries.clear();  // unique_ptr in QueryRecord frees the shapes
}

bool FileCollection::file_exists(std::string& file)
{
  auto it = std::find(files.begin(), files.end(), file);
  return it != files.end();
}

PathType FileCollection::parse_path(const std::string& path)
{
  // Check for EPT endpoint (remote or local ept.json)
  if (is_remote_path(path))
  {
    // Check if URL path ends with ept.json (strip query params first)
    std::string url_path = path.substr(0, path.find('?'));
    if (url_path.size() >= 8 && url_path.substr(url_path.size() - 8) == "ept.json")
      return PathType::REMOTEEPTFILE;

    // Validate remote file has a LAS/LAZ extension
    std::string lower_path = url_path;
    std::transform(lower_path.begin(), lower_path.end(), lower_path.begin(), ::tolower);
    if (lower_path.size() >= 4)
    {
      std::string ext = lower_path.substr(lower_path.size() - 4);
      if (ext == ".las" || ext == ".laz")
        return PathType::REMOTELASFILE;
    }

    return PathType::OTHERFILE;
  }

  std::filesystem::path file_path(path);

  if (std::filesystem::exists(file_path))
  {
    if (std::filesystem::is_regular_file(file_path))
    {
      // Check for local EPT endpoint
      if (file_path.filename() == "ept.json")
        return PathType::EPTFILE;

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


uint64_t FileCollection::get_total_points() const
{
  uint64_t total = 0;
  for (const Header& h : headers) total += h.number_of_point_records;
  return total;
}

FileCollection::FileCollection()
{
  clear();
}

FileCollection::~FileCollection()
{
  // QueryRecord destructor (unique_ptr<Shape>) handles cleanup.
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

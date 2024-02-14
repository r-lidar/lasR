// LASR
#include "LAScatalog.h"
#include "Progress.hpp"
#include "error.h"

// LASlib
#include "lasreader.hpp"
#include "laskdtree.hpp"

// STL
#include <iostream>
#include <algorithm>
#include <filesystem>

// boost
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

namespace pt = boost::property_tree;

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
      read_vpc(file);
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

  if (epsg_set.size() > 1) eprint("WARNING: mix epsg found. First one retained\n");
  if (wkt_set.size() > 1) eprint("WARNING: mix wkt crs found. First one retained.\n");
  if (epsg_set.size() > 0) { wkt = ""; epsg = *epsg_set.begin(); }
  if (wkt_set.size() > 0) { wkt = *wkt_set.begin(); epsg = 0; }

  return true;
}

bool LAScatalog::read_vpc(const std::string& filename)
{
  clear();
  use_dataframe = false;

  // Get the parent file to resolve relative path later
  std::filesystem::path parent_folder = std::filesystem::path(filename).parent_path();

  try
  {
    // read the JSON file
    pt::ptree vpc;
    pt::read_json(filename, vpc);

    // Check that it has a FeatureCollection property
    if (vpc.find("type") ==  vpc.not_found() || vpc.get<std::string>("type") != "FeatureCollection")
    {
      last_error = std::string("The input file is not a virtual point cloud file"); // # nocov
      return false; // # nocov
    }

    // Check that it has a feature property
    if (vpc.find("features") ==  vpc.not_found())
    {
      last_error = "Missing 'features' key in the virtual point cloud file"; // # nocov
      return false; // # nocov
    }

    // Loop over the features to read the file paths and bounding boxes
    for (const auto &f : vpc.get_child("features"))
    {
      auto& feature = f.second;

      if (feature.find("type") ==  feature.not_found() || feature.find("stac_version") == feature.not_found() || feature.find("assets") == feature.not_found() || feature.find("properties") == feature.not_found())
      {
        last_error = "Malformed virtual point cloud file: missing properties"; // # nocov
        return false; // # nocov
      }

      if (feature.get<std::string>("type") != "Feature")
      {
        last_error = "Malformed virtual point cloud file: 'type' is not equal to 'Feature'"; // # nocov
        return false; // # nocov
      }

      if (feature.get_child("assets").empty() || feature.get_child("properties").empty())
      {
        last_error = "Malformed virtual point cloud file: empty 'assets' or 'properties"; // # nocov
        return false; // # nocov
      }

      if (feature.get<std::string>("stac_version")  != "1.0.0")
      {
        last_error =  std::string("Unsupported STAC version: ") + feature.get<std::string>("stac_version"); // # nocov
        return false; // # nocov
      }

      // Find the path to the file and resolve relative path
      auto& first_asset = *feature.get_child("assets").begin();
      std::string file = first_asset.second.get<std::string>("href");
      file = std::filesystem::weakly_canonical(parent_folder / file).string();

      int pccount = feature.get<int>("properties.pc:count");

      // Get the CRS either in WKT or EPSG
      std::string wkt = feature.get<std::string>("properties.proj:wkt2", "");
      int epsg = feature.get<int>("properties.proj:epsg", 0);

      // Read the bounding box. If not found fall back to regular file reading
      auto optbbox = feature.get_child_optional("properties.proj:bbox");
      if (!optbbox)
      {
        if (!add_file(file))
        {
          return false;
        }
        continue;
      }

      // We are sure there is a bbox
      auto& bbox = feature.get_child("properties.proj:bbox");
      auto it = bbox.begin();
      double min_x, min_y, max_x, max_y;
      if (bbox.size() == 6)
      {
        min_x = it->second.get_value<double>(); it++;
        min_y = it->second.get_value<double>(); it++;
        it++; // min_z
        max_x = it->second.get_value<double>(); it++;
        max_y = it->second.get_value<double>(); it++;
      }
      else if (bbox.size() == 4)
      {
        min_x = it->second.get_value<double>(); it++;
        min_y = it->second.get_value<double>(); it++;
        max_x = it->second.get_value<double>(); it++;
        max_y = it->second.get_value<double>();
      }
      else
      {
        last_error = "Malformed virtual point cloud file: proj:bbox should be 2D or 3D"; // # nocov
        return false; // # nocov
      }

      bool spatial_index = feature.get<bool>("properties.index:indexed", false);

      //printf("pc:count %d\n", pccount);
      //printf("pc:wkt %s\n", wkt.c_str());
      //printf("pc:epsg %d\n", epsg);
      //printf("file %s\n", file.c_str());
      //printf("bbox [%.1f %.1f %.1f %.1f]\n", min_x, min_y, max_x, max_y);

      files.push_back(file);
      add_wkt(wkt);
      add_epsg(epsg);
      add_bbox(min_x, min_y, max_x, max_y, spatial_index);
      npoints.push_back(pccount);
    }
  }
  catch (const pt::json_parser_error& e)
  {
    last_error = std::string("JSON parsing error: ") + e.what(); // # nocov
    return false; // # nocov
  }
  catch (const pt::ptree_error& e)
  {
    last_error = std::string("Error reading JSON file: ") + e.what();  // # nocov
    return false;  // # nocov
  }
  catch (const std::exception& e)
  {
    last_error = std::string("An unexpected error occurred in read_vpc(): ") + e.what(); // # nocov
    return false; // # nocov
  }

  return true;
}

bool LAScatalog::write_vpc(const std::string& vpcfile)
{
  if (use_dataframe)
  {
    last_error = "Cannot write a virtual point cloud file with a data.frame"; // # nocov
    return false; // # nocov
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

  // Mmmm.... boost property_tree is not able to write numbers... will do everything by hand
  auto autoquote = [](const std::string& str) { return '"' + str + '"'; };


  output << "{" << std::endl;
  output << "  \"type\": \"FeatureCollection\"," << std::endl;
  output << "  \"features\": [" << std::endl;

  for (int i = 0 ; i < get_number_files() ; i++)
  {
    std::filesystem::path file = files[i];
    std::string relative_path = "./" + std::filesystem::relative(file, output_path).string();
    std::replace(relative_path.begin(), relative_path.end(), '\\', '/'); // avoid problem of backslash on windows

    Rectangle& bbox = bboxes[i];
    uint64_t n = npoints[i];
    bool index = indexed[i];

    output << "  {" << std::endl;
    output << "    \"type\": \"Feature\"," << std::endl;
    output << "    \"stac_version\": \"1.0.0\"," << std::endl;
    output << "    \"stac_extensions\": [" << std::endl;
    output << "      \"https://stac-extensions.github.io/pointcloud/v1.0.0/schema.json\"," << std::endl;
    output << "      \"https://stac-extensions.github.io/projection/v1.1.0/schema.json\"" << std::endl;
    output << "     ]," << std::endl;
    output << "    \"id\": " << autoquote(file.stem().string()) << "," << std::endl;
    output << "    \"properties\": {" << std::endl;
    output << "      \"datatime\": " << "\"0-01-01T00:00:00Z\""<< "," << std::endl;
    output << "      \"pc:count\": " << n << "," << std::endl;
    output << "      \"pc:encoding\": " << "\"?\""<< "," << std::endl;
    output << "      \"pc:type\": " << "\"lidar\"" << ","<< std::endl;
    output << "      \"proj:bbox\": [" << std::fixed << std::setprecision(3) << bbox.minx << ", " << bbox.miny << ", " << bbox.maxx << ", " << bbox.maxy << "],"<< std::endl;
    if (!wkt.empty()) output << "      \"proj:wtk2\": " << wkt << "," << std::endl;
    else if (epsg != 0) output << "      \"proj:epsg\": " << epsg << "," << std::endl;
    output << "      \"index:indexed\": " << index << std::endl;
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
    add_wkt(std::string(header->vlr_geo_ogc_wkt));
  }
}

bool LAScatalog::add_file(const std::string& file, bool noprocess)
{
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
  npoints.push_back(MAX(lasreader->header.number_of_point_records, lasreader->header.number_of_extended_variable_length_records));

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

void LAScatalog::set_chunk_size(double size)
{
  if (size > 0)
    chunk_size = size;
  else
    chunk_size = 0;
}

void LAScatalog::set_chunk_is_file()
{
  chunk_size = 0;
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
    last_error = "internal error: spatial index of tile not built";
  }

  bool success = false;

  if (queries.size() == 0)
    success = get_chunk_regular(i, chunk);
  else
    success = get_chunk_with_query(i, chunk);

  chunk.process = !noprocess[i];

  return success;
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

  // No buffer, we don't need to go further there is not other file to read
  if (buffer <= 0)
  {
    return true;
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
  unsigned int index;
  chunk.clear();

  // Some shape are provided. We are performing queries i.e not processing the entire collection
  Shape* q = queries[i];
  double minx = q->xmin();
  double miny = q->ymin();
  double maxx = q->xmax();
  double maxy = q->ymax();
  double centerx = q->centroid().x;
  double centery = q->centroid().y;
  double epsilon = 1e-8;

  // Search if there is a match
  laskdtree->overlap(minx, miny,  maxx, maxy);
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
    chunk.name = files[0].stem().string();
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
    if (chunk.name.empty()) files[index].stem().string() + "_" + std::to_string(i);
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
  epsg = 0;
  wkt.clear();

  use_dataframe = true;

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




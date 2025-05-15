#include "PCDio.h"
#include "Header.h"
#include "PointSchema.h"
#include "Progress.h"

#include <sstream>
#include <limits>
#include <iomanip> // For std::setprecision

PCDio::PCDio()
{
  header = nullptr;
  preread_bbox = false;
  is_binary = false;
  npoints = 0;
}

PCDio::PCDio(Progress* progress)
{
  header = nullptr;
  preread_bbox = false;
  is_binary = false;
  npoints = 0;

  this->progress = progress;
}

PCDio::~PCDio()
{
  close();
}

bool PCDio::open(const Chunk& chunk, std::vector<std::string> filters)
{
  if (ostream.is_open())
  {
    last_error = "Internal error. This interface has been created as a writer"; // # nocov
    return false;
  }

  return open(chunk.main_files[0]);
}

bool PCDio::open(const std::string& file)
{
  if (header != nullptr)
  {
    last_error = "Internal error. Header already initialized."; // # nocov
    return false;
  }

  if (ostream.is_open())
  {
    last_error = "Internal error. This interface has been created as a writer"; // # nocov
    return false;
  }

  this->file = file;
  istream.open(file, std::ios::binary);

  return true;
}

bool PCDio::populate_header(Header* header, bool read_first_point)
{
  if (!istream.is_open())
  {
    last_error = "Internal error. PCDreader not initialized."; // # nocov
    return false;
  }

  std::string line;

  int targetIndex = -1;
  size_t pointCount = 0;

  std::vector<std::string> fields;
  std::vector<int> data_sizes;
  std::vector<std::string> data_types;
  std::vector<int> data_counts;
  unsigned int npoints;
  std::string data;
  unsigned char version_major = 0;
  unsigned char version_minor = 0;

  while (std::getline(istream, line))
  {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream lineStream(line);
    std::string key;
    lineStream >> key;

    if (key == "VERSION")
    {
      std::string version;
      lineStream >> version;

      size_t dotPos = version.find('.');
      int major = 0, minor = 0;

      if (dotPos == 0)
      {
        // Case: ".y" -> Major = 0, Minor = y
        version_minor = std::stoi(version.substr(1));
      }
      else if (dotPos != std::string::npos)
      {
        // Case: "x.y"
        version_major = std::stoi(version.substr(0, dotPos));
        version_minor = std::stoi(version.substr(dotPos + 1));
      }
      else
      {
        // Case: "x" (no dot) -> Major = 2, Minor = 0
        version_major = std::stoi(version);
      }
    }
    else if (key == "FIELDS")
    {
      std::string field;
      while (lineStream >> field) { fields.push_back(field); }
    }
    else if (key == "SIZE")
    {
      int size;
      while (lineStream >> size) { data_sizes.push_back(size); }
    }
    else if (key == "TYPE")
    {
      std::string type;
      while (lineStream >> type) { data_types.push_back(type); }
    }
    else if (key == "COUNT")
    {
      int count;
      while (lineStream >> count) { data_counts.push_back(count); }
    }
    else if (key == "WIDTH")
    {
      lineStream >> npoints;
    }
    else if (key == "HEIGHT")
    {
      //lineStream >> header->height;
    }
    else if (key == "VIEWPOINT")
    {
      float vp[7];
      for (int i = 0; i < 7; ++i)
      {
        if (!(lineStream >> vp[i])) {
          throw std::runtime_error("Error parsing VIEWPOINT");
        }
      }
    }
    else if (key == "POINTS")
    {
      lineStream >> npoints;
    }
    else if (key == "DATA")
    {
      lineStream >> data;
      break;
    }
    else
    {
      last_error = "Unknown header key: " + key;
      return false;
    }
  }

  if (data != "ascii" && data != "binary")
  {
    last_error = "Unsupported data format: " + data;
    return false;
  }

  if (data == "binary")
  {
    is_binary = true;
    read = &PCDio::read_binary_point;
  }
  else
  {
    read = &PCDio::read_ascii_point;
  }

  if (fields.size() < 3)
  {
    last_error = "The files must have at least 3 fields";
    return false;
  }

  if (fields[0] != "x" && fields[0] != "X")
  {
    last_error = "The first field must be 'x' or 'X' not '" + fields[0] + "'";
    return false;
  }

  if (fields[1] != "y" && fields[1] != "Y")
  {
    last_error = "The second field must be 'y' or 'Y' not '" + fields[1] + "'";
    return false;
  }

  if (fields[2] != "z" && fields[2] != "Z")
  {
    last_error = "The third field must be 'z' or 'Z' not '" + fields[2] + "'";
    return false;
  }

  Attribute attrf("flags", AttributeType::INT8, 1, 0, "Internal 8-bit mask reserved lasR core engine");
  header->add_attribute(attrf);

  for (int i = 0 ; i < fields.size() ; i++)
  {
    std::string name = fields[i];
    name = map_attribute(name);

    AttributeType type = AttributeType::NOTYPE;
    if (data_types[i] == "I" && data_sizes[i] == 1)
      type = AttributeType::INT8;
    else if (data_types[i] == "I" && data_sizes[i] == 2)
      type = AttributeType::INT16;
    else if (data_types[i] == "I" && data_sizes[i] == 4)
      type = AttributeType::INT32;
    else if (data_types[i] == "I" && data_sizes[i] == 8)
      type = AttributeType::INT64;
    else if (data_types[i] == "U" && data_sizes[i] == 1)
      type = AttributeType::UINT8;
    else if (data_types[i] == "U" && data_sizes[i] == 2)
      type = AttributeType::UINT16;
    else if (data_types[i] == "U" && data_sizes[i] == 4)
      type = AttributeType::UINT32;
    else if (data_types[i] == "U" && data_sizes[i] == 8)
      type = AttributeType::UINT64;
    else if (data_types[i] == "F" && data_sizes[i] == 4)
      type = AttributeType::FLOAT;
    else if (data_types[i] == "F" && data_sizes[i] == 8)
      type = AttributeType::DOUBLE;
    else
    {
      last_error = "Unsupported data type " + data_types[i] + std::to_string(data_sizes[i]);
      return false;
    }

    Attribute attr(name, type);
    header->add_attribute(attr);
  }

  this->header = header;

  header->signature = "PCDF";
  header->version_major = version_major;
  header->version_minor = version_minor;
  header->number_of_point_records = npoints;
  header->min_x = std::numeric_limits<double>::infinity();
  header->min_y = std::numeric_limits<double>::infinity();
  header->min_z = std::numeric_limits<double>::infinity();
  header->max_x = -std::numeric_limits<double>::infinity();
  header->max_y = -std::numeric_limits<double>::infinity();
  header->max_z = -std::numeric_limits<double>::infinity();

  // Check for a .bbox file
  std::string bbox_filename = file.substr(0, file.find_last_of('.')) + ".bbox";

  // Read the bbox from the bbox file. It not then, compute the bbox by reading the file.
  if (!read_bbox(bbox_filename) && preread_bbox)
  {
    auto payload_start = istream.tellg();

    Point p(&header->schema);
    while (read_point(&p))
    {
      if (header->min_x > p.get_x()) header->min_x = p.get_x();
      if (header->min_y > p.get_y()) header->min_y = p.get_y();
      if (header->min_z > p.get_z()) header->min_z = p.get_z();
      if (header->max_x < p.get_x()) header->max_x = p.get_x();
      if (header->max_y < p.get_y()) header->max_y = p.get_y();
      if (header->max_z < p.get_z()) header->max_z = p.get_z();
    }

    istream.clear();
    istream.seekg(payload_start);

    // Write the bounding box to the .bbox file
    write_bbox(bbox_filename);
    npoints = 0;
  }

  return true;
}

bool PCDio::read_point(Point* p)
{
  return (this->*read)(p);
}

bool PCDio::read_binary_point(Point* p)
{
  p->zero();
  istream.read(reinterpret_cast<char*>(p->data + 1), p->schema->total_point_size-1);  // + 1 byte because of the flags used by lasR
  if (istream.gcount() != header->schema.total_point_size-1) return false;   // Check if the correct number of bytes were read
  npoints++;
  return true;
}

bool PCDio::read_ascii_point(Point* p)
{
  p->zero();
  if (istream.eof()) return false;

  std::string line;
  if (!std::getline(istream, line))
  {
    last_error = "Fail to read line";
    return false;
  }

  std::istringstream line_stream(line);
  for (int i = 1; i < header->schema.num_attributes(); ++i)
  {
    const auto& attr = header->schema.attributes[i];
    if (!parse_attribute(line_stream, attr.type, p->data + attr.offset))
    {
      last_error = "Failed to parse " + attr.name;
      return false;
    }
  }

  npoints++;
  return true;
}

bool PCDio::parse_attribute(std::istringstream& line_stream, AttributeType type, void* dest)
{
  if (!dest) return false; // Null pointer check
  switch (type)
  {
    case AttributeType::FLOAT:  return static_cast<bool>(line_stream >> *static_cast<float*>(dest));
    case AttributeType::DOUBLE: return static_cast<bool>(line_stream >> *static_cast<double*>(dest));
    case AttributeType::INT8:   return static_cast<bool>(line_stream >> *reinterpret_cast<int8_t*>(dest));
    case AttributeType::INT16:  return static_cast<bool>(line_stream >> *reinterpret_cast<int16_t*>(dest));
    case AttributeType::INT32:  return static_cast<bool>(line_stream >> *reinterpret_cast<int32_t*>(dest));
    case AttributeType::INT64:  return static_cast<bool>(line_stream >> *reinterpret_cast<int64_t*>(dest));
    case AttributeType::UINT8:  return static_cast<bool>(line_stream >> *reinterpret_cast<uint8_t*>(dest));
    case AttributeType::UINT16: return static_cast<bool>(line_stream >> *reinterpret_cast<uint16_t*>(dest));
    case AttributeType::UINT32: return static_cast<bool>(line_stream >> *reinterpret_cast<uint32_t*>(dest));
    case AttributeType::UINT64: return static_cast<bool>(line_stream >> *reinterpret_cast<uint64_t*>(dest));
    default: return false; // Unsupported type
  }
}

bool PCDio::is_opened()
{
  return (istream.is_open() || ostream.is_open());
}

int64_t PCDio::p_count()
{
  return npoints;
}

void PCDio::close()
{
  if (istream.is_open())
  {
    istream.close();
    header = nullptr;
  }

  if (ostream.is_open())
  {
    ostream.close();
    header = nullptr;
  }
}

bool PCDio::write_bbox(const std::string &bbox_filename)
{
  std::ofstream bbox_out(bbox_filename, std::ios::binary);
  if (!bbox_out.is_open())
  {
    last_error = "Failed to open bbox file for writing.\n";
    return false;
  }

  // Write ASCII format
  bbox_out << std::setprecision(4) << header->min_x << " " << header->min_y << " " << header->min_z << " " << header->max_x << " " << header->max_y << " " << header->max_z << std::endl;

  // Marker for binary start
  bbox_out << "BINARY:\n";

  // Write binary format
  bbox_out.write(reinterpret_cast<const char*>(&header->min_x), sizeof(header->min_x));
  bbox_out.write(reinterpret_cast<const char*>(&header->min_y), sizeof(header->min_y));
  bbox_out.write(reinterpret_cast<const char*>(&header->min_z), sizeof(header->min_z));
  bbox_out.write(reinterpret_cast<const char*>(&header->max_x), sizeof(header->max_x));
  bbox_out.write(reinterpret_cast<const char*>(&header->max_y), sizeof(header->max_y));
  bbox_out.write(reinterpret_cast<const char*>(&header->max_z), sizeof(header->max_z));

  bbox_out.close();

  return true;
}

bool PCDio::read_bbox(const std::string &bbox_filename)
{
  std::ifstream bbox_file(bbox_filename, std::ios::binary);
  if (!bbox_file.is_open()) return false;

  // Locate the binary section
  std::string line;
  while (std::getline(bbox_file, line))
  {
    if (line == "BINARY") break;
  }

  if (bbox_file.eof())
  {
    last_error = "BINARY marker not found in bbox file.\n";
    return false;
  }

  // Read binary data
  bbox_file.read(reinterpret_cast<char*>(&header->min_x), sizeof(header->min_x));
  bbox_file.read(reinterpret_cast<char*>(&header->min_y), sizeof(header->min_y));
  bbox_file.read(reinterpret_cast<char*>(&header->min_z), sizeof(header->min_z));
  bbox_file.read(reinterpret_cast<char*>(&header->max_x), sizeof(header->max_x));
  bbox_file.read(reinterpret_cast<char*>(&header->max_y), sizeof(header->max_y));
  bbox_file.read(reinterpret_cast<char*>(&header->max_z), sizeof(header->max_z));

  bbox_file.close();
  return true;
}

void PCDio::reset_accessor()
{

}

bool PCDio::create(const std::string& file)
{
  last_error = "Writing PCD file is not supported yet";
  return false;
}

bool PCDio::init(const Header* header)
{
  last_error = "Writing PCD file is not supported yet";
  return false;
}
bool PCDio::write_point(Point* p)
{
  last_error = "Writing PCD file is not supported yet";
  return false;
}


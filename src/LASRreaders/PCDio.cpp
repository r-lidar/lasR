#include "PCDio.h"
#include "Header.h"
#include "PointSchema.h"
#include "Progress.h"

#include <limits>
#include <iomanip> // For std::setprecision
#include <sstream>

#define EPSILON 1e-9

PCDio::PCDio()
{
  header = nullptr;
  preread_bbox = true;
  is_binary = false;
  npoints = 0;
}

PCDio::PCDio(Progress* progress)
{
  header = nullptr;
  preread_bbox = true;
  is_binary = false;
  npoints = 0;

  this->progress = progress;
}

PCDio::~PCDio()
{
  close();
}

void PCDio::query(const std::vector<std::string>& main_files,
                  const std::vector<std::string>& neighbour_files,
                  double xmin, double ymin, double xmax, double ymax,
                  double buffer, bool circle,
                  std::vector<std::string> filters)
{
  if (ostream.is_open())
    throw std::logic_error("Internal error: This interface has been created as a writer");

  if (main_files.size() > 1)
    throw std::invalid_argument("PCD file reader cannot read multiple PCD files yet");

  if (!neighbour_files.empty())
    throw std::invalid_argument("PCD file reader cannot read buffered PCD files yet");

  if ((filters.size() == 1 && !filters[0].empty()) || filters.size() > 1)
    throw std::invalid_argument("Filters are not enabled for PCD files yet.");

  open(main_files[0]);
}

void PCDio::open(const std::string& file)
{
  if (header != nullptr)
    throw std::logic_error("Internal error: header already initialized");

  if (ostream.is_open())
    throw std::logic_error("Internal error: this interface was created as a writer");

  this->file = file;
  istream.open(file, std::ios::binary);

  if (!istream.is_open())
    throw std::runtime_error("Failed to open PCD file: " + file);
}

void PCDio::populate_header(Header* header, bool read_first_point)
{
  if (!istream.is_open())
    throw std::logic_error("Internal error. PCDreader not initialized."); // # nocov

  std::string line;

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
      throw std::runtime_error("Unknown header key: " + key);
    }
  }

  if (data != "ascii" && data != "binary")
    throw std::runtime_error("Unsupported data format: " + data);

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
    throw std::runtime_error("The files must have at least 3 fields");

  if (fields[0] != "x" && fields[0] != "X")
    throw std::runtime_error("The first field must be 'x' or 'X' not '" + fields[0] + "'");

  if (fields[1] != "y" && fields[1] != "Y")
    throw std::runtime_error("The second field must be 'y' or 'Y' not '" + fields[1] + "'");

  if (fields[2] != "z" && fields[2] != "Z")
    throw std::runtime_error("The third field must be 'z' or 'Z' not '" + fields[2] + "'");

  Attribute attrf("flags", AttributeType::INT8, 1, 0, "Internal 8-bit mask reserved lasR core engine");
  header->add_attribute(attrf);

  for (unsigned int i = 0 ; i < fields.size() ; i++)
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
      throw std::runtime_error("Unsupported data type " + data_types[i] + std::to_string(data_sizes[i]));
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
  if (!read_bbox(bbox_filename, header) && preread_bbox)
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
    write_bbox(header, bbox_filename);
    npoints = 0;
  }
}

bool PCDio::read_point(Point* p)
{
  return (this->*read)(p);
}

bool PCDio::read_binary_point(Point* p)
{
  p->zero();
  istream.read(reinterpret_cast<char*>(p->data + 1), p->schema->total_point_size-1);  // + 1 byte because of the flags used by lasR
  if (istream.gcount() != (long)header->schema.total_point_size-1) return false;      // Check if the correct number of bytes were read
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
    if (istream.eof()) return false;
    throw std::runtime_error("I/O error while reading line in PCD file");
  }

  std::istringstream iss(line);
  std::vector<double> numbers;
  numbers.reserve(header->schema.attributes.size());
  double num;
  while (iss >> num) numbers.push_back(num);

  if (numbers.size() != header->schema.attributes.size()-1)
    throw std::runtime_error("Invalid number of attribute in line " + line);

  for (int i = 1; i < header->schema.num_attributes(); ++i)
  {
    const auto& attr = header->schema.attributes[i];
    unsigned char* dest = p->data + attr.offset;
    double value = numbers[i - 1];

    switch (attr.type)
    {
      case AttributeType::FLOAT:   *reinterpret_cast<float*>(dest)   = static_cast<float>(value); break;
      case AttributeType::DOUBLE:  *reinterpret_cast<double*>(dest)  = value; break;
      case AttributeType::INT8:    *reinterpret_cast<int8_t*>(dest)  = static_cast<int8_t>(value); break;
      case AttributeType::INT16:   *reinterpret_cast<int16_t*>(dest) = static_cast<int16_t>(value); break;
      case AttributeType::INT32:   *reinterpret_cast<int32_t*>(dest) = static_cast<int32_t>(value); break;
      case AttributeType::INT64:   *reinterpret_cast<int64_t*>(dest) = static_cast<int64_t>(value); break;
      case AttributeType::UINT8:   *reinterpret_cast<uint8_t*>(dest) = static_cast<uint8_t>(value); break;
      case AttributeType::UINT16:  *reinterpret_cast<uint16_t*>(dest)= static_cast<uint16_t>(value); break;
      case AttributeType::UINT32:  *reinterpret_cast<uint32_t*>(dest)= static_cast<uint32_t>(value); break;
      case AttributeType::UINT64:  *reinterpret_cast<uint64_t*>(dest)= static_cast<uint64_t>(value); break;
      default: throw std::runtime_error("Unsupported attribute type");
    }
  }

  npoints++;
  return true;
}

bool PCDio::write_point(Point* p)
{
  return (this->*write)(p);
}

bool PCDio::write_ascii_point(Point* p)
{
  if (!ostream.is_open()) return false;

  ostream << std::fixed << std::setprecision(8);
  ostream << p->get_x() << " ";
  ostream << p->get_y() << " ";
  ostream << p->get_z();

  for (int i = 4; i < p->schema->num_attributes(); ++i)
  {
    ostream << " ";
    const Attribute& attr = p->schema->attributes[i];
    void* ptr = (void*)(p->data + attr.offset);

    if (p->schema->attributes[i].type == AttributeType::BIT)
      continue;

    switch (p->schema->attributes[i].type)
    {
      case AttributeType::FLOAT:  ostream << *reinterpret_cast<float*>(ptr); break;
      case AttributeType::DOUBLE: ostream << *reinterpret_cast<double*>(ptr); break;
      case AttributeType::INT8:   ostream << static_cast<int>(*reinterpret_cast<char*>(ptr)); break;
      case AttributeType::INT16:  ostream << *reinterpret_cast<short*>(ptr); break;
      case AttributeType::INT32:  ostream << *reinterpret_cast<int*>(ptr); break;
      case AttributeType::INT64:  ostream << *reinterpret_cast<int64_t*>(ptr); break;
      case AttributeType::UINT8:  ostream << static_cast<unsigned int>(*reinterpret_cast<unsigned char*>(ptr)); break;
      case AttributeType::UINT16: ostream << *reinterpret_cast<unsigned short*>(ptr); break;
      case AttributeType::UINT32: ostream << *reinterpret_cast<unsigned int*>(ptr); break;
      case AttributeType::UINT64: ostream << *reinterpret_cast<uint64_t*>(ptr); break;
      default: ostream << "NaN";
    }
  }
  ostream << std::endl;
  npoints++;
  return true;
}

bool PCDio::write_binary_point(Point* p)
{
  if (!ostream.is_open()) return false;

  const auto& schema = p->schema->attributes;

  // The coordinates can be either
  // - int (from LAS) with scale factor and offset
  // - double (from PCD)
  // - float (from PCD)
  // We need to respect the type at write time

  double x = p->get_x();
  if (schema[AttributeCore::X].type == AttributeType::FLOAT)
  {
    float xf = static_cast<float>(x);
    ostream.write(reinterpret_cast<const char*>(&xf), sizeof(float));
  }
  else
  {
    ostream.write(reinterpret_cast<const char*>(&x), sizeof(double));
  }

  double y = p->get_y();
  if (schema[AttributeCore::Y].type == AttributeType::FLOAT)
  {
    float yf = static_cast<float>(y);
    ostream.write(reinterpret_cast<const char*>(&yf), sizeof(float));
  }
  else
  {
    ostream.write(reinterpret_cast<const char*>(&y), sizeof(double));
  }


  double z = p->get_z();
  if (schema[AttributeCore::Z].type == AttributeType::FLOAT)
  {
    float zf = static_cast<float>(z);
    ostream.write(reinterpret_cast<const char*>(&zf), sizeof(float));
  }
  else
  {
    ostream.write(reinterpret_cast<const char*>(&z), sizeof(double));
  }

  for (int i = 4; i < p->schema->num_attributes(); ++i)
  {
    const Attribute& attr = p->schema->attributes[i];
    void* ptr = (void*)(p->data + attr.offset);

    if (attr.type == AttributeType::BIT)
      continue;

    switch (attr.type)
    {
      case AttributeType::FLOAT:  ostream.write(reinterpret_cast<const char*>(ptr), sizeof(float)); break;
      case AttributeType::DOUBLE: ostream.write(reinterpret_cast<const char*>(ptr), sizeof(double)); break;
      case AttributeType::INT8:   ostream.write(reinterpret_cast<const char*>(ptr), sizeof(int8_t)); break;
      case AttributeType::INT16:  ostream.write(reinterpret_cast<const char*>(ptr), sizeof(int16_t)); break;
      case AttributeType::INT32:  ostream.write(reinterpret_cast<const char*>(ptr), sizeof(int32_t)); break;
      case AttributeType::INT64:  ostream.write(reinterpret_cast<const char*>(ptr), sizeof(int64_t)); break;
      case AttributeType::UINT8:  ostream.write(reinterpret_cast<const char*>(ptr), sizeof(uint8_t)); break;
      case AttributeType::UINT16: ostream.write(reinterpret_cast<const char*>(ptr), sizeof(uint16_t)); break;
      case AttributeType::UINT32: ostream.write(reinterpret_cast<const char*>(ptr), sizeof(uint32_t)); break;
      case AttributeType::UINT64: ostream.write(reinterpret_cast<const char*>(ptr), sizeof(uint64_t)); break;
      default:
        // Optionally write NaN or skip
        break;
    }
  }

  npoints++;
  return true;
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

bool PCDio::write_bbox(const Header* header, const std::string &bbox_filename)
{
  std::ofstream bbox_out(bbox_filename, std::ios::binary);
  if (!bbox_out.is_open())
    throw std::runtime_error("Failed to open bbox file for writing.");

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

bool PCDio::read_bbox(const std::string &bbox_filename, Header* header)
{
  std::ifstream bbox_file(bbox_filename, std::ios::binary);
  if (!bbox_file.is_open()) return false;

  // Locate the binary section
  std::string line;
  while (std::getline(bbox_file, line))
  {
    if (line == "BINARY:") break;
  }

  if (bbox_file.eof())
    throw std::runtime_error("BINARY marker not found in bbox file.");

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

void PCDio::create(const std::string& file)
{
  if (istream.is_open())
    throw std::runtime_error("This PCDio instance is configured for reading, not writing.");

  this->file = file;

  if (is_binary)
    ostream.open(file, std::ios::out | std::ios::binary);
  else
    ostream.open(file);

  if (!ostream.is_open())
    throw std::runtime_error("Failed to open file for writing: " + file);

  const AttributeSchema& schema = header->schema;

  ostream << "# .PCD v0.7 - Point Cloud Data file format\n";
  ostream << "VERSION 0.7\n";

  ostream << "FIELDS";
  ostream << " x y z";
  for (int i = 4; i < schema.num_attributes(); ++i)
  {
    if (schema.attributes[i].type == AttributeType::BIT) continue;
    ostream << " " << schema.attributes[i].name;
  }
  ostream <<  std::endl;

  ostream << "SIZE";
  if (schema.attributes[1].type == AttributeType::INT32) // LAS format
    ostream << " 8 8 8";
  else
    ostream << " 4 4 4";
  for (int i = 4; i < schema.num_attributes(); ++i)
  {
    if (schema.attributes[i].type == AttributeType::BIT) continue;
    ostream << " " << schema.attributes[i].size;
  }
  ostream << std::endl;

  ostream << "TYPE";
  ostream << " F F F";
  for (int i = 4; i < schema.num_attributes(); ++i)
  {
    if (schema.attributes[i].type == AttributeType::BIT) continue;
    ostream << " " << attribute_type_code(schema.attributes[i].type);
  }
  ostream << std::endl;

  ostream << "COUNT";
  for (int i = 1; i < schema.num_attributes(); ++i)
  {
    if (schema.attributes[i].type == AttributeType::BIT) continue;
    ostream << " 1";
  }
  ostream << std::endl;

  ostream << "WIDTH " << header->number_of_point_records << "\n";
  ostream << "HEIGHT 1\n";
  ostream << "VIEWPOINT 0 0 0 1 0 0 0\n";
  ostream << "POINTS " << header->number_of_point_records << "\n";

  if (is_binary)
    ostream << "DATA binary\n";
  else
    ostream << "DATA ascii\n";

  if (is_binary)
    write = &PCDio::write_binary_point;
  else
    write = &PCDio::write_ascii_point;

  npoints = 0;
}


void PCDio::init(const Header* header)
{
  this->header = header;
}

char PCDio::attribute_type_code(AttributeType type)
{
  switch (type)
  {
  case AttributeType::FLOAT:
  case AttributeType::DOUBLE:
    return 'F';
  case AttributeType::INT8:
  case AttributeType::INT16:
  case AttributeType::INT32:
  case AttributeType::INT64:
    return 'I';
  case AttributeType::UINT8:
  case AttributeType::UINT16:
  case AttributeType::UINT32:
  case AttributeType::UINT64:
    return 'U';
  default:
    return '?';
  }
}

void PCDio::set_binary_mode(bool b)
{
  is_binary = b;
}


#include "PCDio.h"
#include "Progress.h"

#include <sstream>
#include <limits>

PCDio::PCDio()
{
  header = nullptr;
  progress = nullptr;
  is_binary = false;
}

PCDio::PCDio(Progress* progress)
{
  this->progress = progress;
  header = nullptr;
  is_binary = false;
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

  istream.open(file, std::ios::binary);

  return true;
}

bool PCDio::populate_header(Header* header)
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
  }

  if (fields.size() < 3)
  {
    last_error = "The files must have at least 3 fields";
    return false;
  }

  if (fields[0] != "x" && fields[0] != "X")
  {
    last_error = "The first fields must be 'x' or 'X' not '" + fields[0] + "'";
    return false;
  }

  if (fields[1] != "y" && fields[1] != "Y")
  {
    last_error = "The first fields must be 'y' or 'Y' not '" + fields[1] + "'";
    return false;
  }

  if (fields[2] != "z" && fields[2] != "Z")
  {
    last_error = "The first fields must be 'z' or 'Z' not '" + fields[2] + "'";
    return false;
  }

  for (int i = 0 ; i < fields.size() ; i++)
  {
    std::string name = fields[i];
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

  header->signature = "PCDF";
  header->number_of_point_records = npoints;
  header->min_x = std::numeric_limits<double>::infinity();
  header->min_y = std::numeric_limits<double>::infinity();
  header->min_z = std::numeric_limits<double>::infinity();
  header->max_x = -std::numeric_limits<double>::infinity();
  header->max_y = -std::numeric_limits<double>::infinity();
  header->max_z = -std::numeric_limits<double>::infinity();

  this->header = header;

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

  return true;
}

bool PCDio::read_point(Point* p)
{
  do
  {
    // Read one point from the stream
    istream.read(reinterpret_cast<char*>(p->data), p->schema->total_point_size);

    // Check if the correct number of bytes were read
    if (istream.gcount() == header->schema.total_point_size)
      return true;
    else
      return false;

  } while (true);
}

bool PCDio::is_opened()
{
  return (istream.is_open() || ostream.is_open());
}

int64_t PCDio::p_count()
{
  return 0;
  //return counter;
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

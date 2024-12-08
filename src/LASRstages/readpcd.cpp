#include "readpcd.h"

LASRpcdreader::LASRpcdreader()
{
  header = nullptr;
  is_binary = false;
}

LASRpcdreader::LASRpcdreader(const LASRpcdreader& other)
{
  header = nullptr;
  is_binary = false;
}

bool LASRpcdreader::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);
  stream.open(chunk.main_files[0], std::ios::binary);

  if (!stream.is_open())
  {
    last_error = "Unable to open file: " + chunk.main_files[0];
    return false;
  }

  return true;
}

bool LASRpcdreader::process(Header*& header)
{
  if (header != nullptr) return true;

  header = new Header;
  this->header = header;

  std::string line;
  bool isBinary = false;
  int targetIndex = -1;
  size_t pointCount = 0;

  std::vector<std::string> fields;
  std::vector<int> data_sizes;
  std::vector<std::string> data_types;
  std::vector<int> data_counts;
  unsigned int npoints;
  std::string data;

  while (std::getline(stream, line))
  {
    if (line.empty() || line[0] == '#') continue;
    std::istringstream lineStream(line);
    std::string key;
    lineStream >> key;

    if (key == "FIELDS")
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
    }
    else
    {
      throw std::runtime_error("Unknown header key: " + key);
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

  if (fields[0] != "x" || fields[0] != "X")
  {
    last_error = "The first fields must be 'x'' or 'X'";
    return false;
  }

  if (fields[1] != "y" || fields[1] != "Y")
  {
    last_error = "The first fields must be 'y'' or 'Y'";
    return false;
  }

  if (fields[1] != "z" || fields[1] != "Z")
  {
    last_error = "The first fields must be 'z'' or 'Z'";
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

    Point* p = nullptr;
    while (process(p))
    {
      if (header->min_x > p->get_x()) header->min_x = p->get_x();
      if (header->min_y > p->get_x()) header->min_y = p->get_y();
      if (header->min_z > p->get_x()) header->min_z = p->get_z();
      if (header->max_x < p->get_x()) header->max_x = p->get_x();
      if (header->max_y < p->get_x()) header->max_y = p->get_y();
      if (header->max_z < p->get_x()) header->max_z = p->get_z();
    }
  }

  return true;
}

bool LASRpcdreader::process(Point*& point)
{
  if (point == nullptr)
    point = new Point(&header->schema);

  // Stream data until there are no more points or point is filtered out
  do
  {
    // Read one point from the stream
    stream.read(reinterpret_cast<char*>(point->data), header->schema.total_point_size);

    // Check if the correct number of bytes were read
    if (stream.gcount() == header->schema.total_point_size)
    {
      if (point->inside_buffer(xmin, ymin, xmax, ymax, circular))
        point->set_buffered();

    }
    else
    {
      // If no more points (end of file or error), exit streaming mode
      delete point;
      point = nullptr;
      break; // Exit the loop if we've reached the end of the file or an error
    }
  } while (point != nullptr && pointfilter.filter(point));

  return true;
}


// In memory mode
bool LASRpcdreader::process(PointCloud*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new PointCloud(header);

  progress->reset();
  progress->set_total(header->number_of_point_records);
  progress->set_prefix("read_las");

  Point p(&header->schema);

  if (is_binary)
  {
    int totalSize = header->schema.total_point_size;

    std::vector<char> buffer(totalSize);
    for (size_t i = 0; i < header->number_of_point_records; ++i)
    {
      stream.read(reinterpret_cast<char*>(p.data), totalSize);
      if (stream.gcount() != totalSize)
      {
        last_error = "Error reading binary data";
        return false;
      }

      p.dump();
    }
  }

  progress->done();

  if (verbose) print(" Number of point read %d\n", las->npoints);

  return true;
}

LASRpcdreader::~LASRpcdreader()
{
}

void LASRpcdreader::clear(bool)
{
  stream.close();

  // Called at the end of the pipeline. We can delete the header
  /*if (streaming && header)
  {
    delete header;
    header = nullptr;
  }*/
}
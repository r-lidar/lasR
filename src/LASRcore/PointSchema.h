#ifndef POINTSCHEMA_H
#define POINTSCHEMA_H

#include "CRS.h"

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <algorithm>

static const std::unordered_map<std::string, std::vector<std::string>> attribute_map = {
  {"z", {"Z", "z"}},
  {"i", {"Intensity", "intensity", "i"}},
  {"r", {"return", "Return", "ReturnNumber", "return_number", "r"}},
  {"n", {"NumberOfReturn", "NumberReturn", "numberofreturn", "n"}},
  {"c", {"Classification", "classification", "class", "c"}},
  {"t", {"gpstime", "gps_time", "GPStime", "t", "time", "gps"}},
  //s
  //k
  //w
  {"u", {"UserData", "userdata", "user_data", "ud", "u"}},
  {"p", {"PointSourceID", "point_source", "point_source_id", "pointsourceid", "psid", "p"}},
  //e
  //d
  {"a", {"angle", "Angle", "ScanAngle", "ScanAngleRank", "scan_angle", "a"}},
  {"R", {"R", "Red", "red"}},
  {"G", {"G", "Green", "green"}},
  {"B", {"B", "Blue", "blue"}},
  {"N", {"N", "NIR", "nir"}},
};

static std::string map_attribute(const std::string& attribute)
{
  for (const auto& [standard_name, aliases] : attribute_map)
  {
    if (std::find(aliases.begin(), aliases.end(), attribute) != aliases.end())
    {
      return standard_name;
    }
  }

  return attribute;
}


enum AttributeType {
  NOTYPE = -1,
  UINT8 = 0,
  INT8 = 1,
  UINT16 = 2,
  INT16 = 3,
  UINT32 = 4,
  INT32 = 5,
  UINT64 = 6,
  INT64 = 7,
  FLOAT = 8,
  DOUBLE = 9
};

struct Attribute
{
  std::string name;       // Attribute name
  size_t offset;          // Offset in the byte array
  size_t size;            // Size of the attribute in bytes
  AttributeType type;     // Type of the attribute
  double scale_factor;    // Scaling factor
  double value_offset;    // Value offset
  std::string description;
};

struct AttributeSchema
{
  AttributeSchema() { total_point_size = 0; }
  std::vector<Attribute> attributes;
  size_t total_point_size;

  void add_attribute(const std::string& name, AttributeType type, double scale_factor = 1.0, double value_offset = 0.0, std::string description = "")
  {
    size_t size;
    switch (type) {
    case UINT8:
    case INT8:
      size = 1; break;
    case UINT16:
    case INT16:
      size = 2; break;
    case UINT32:
    case INT32:
    case UINT64:
      size = 4; break;
    case INT64:
    case DOUBLE:
      size = 8; break;
    default:
      size = 0; // Invalid type, handle error if needed
    }

    attributes.push_back({name, total_point_size, size, type, scale_factor, value_offset, description});
    total_point_size += size;
  }

  void dump()
  {
    for (const auto& attr : attributes)
    {
      printf("Attribute Name: %-10s | Offset: %-2zu | Size: %-1zu | Type: %-1d | Scale Factor: %-5.2f | Offset: %-5.2f\n", attr.name.c_str(), attr.offset, attr.size, attr.type, attr.scale_factor, attr.value_offset);
    }
  }
};

struct Point
{
  bool own_data;
  unsigned char* data;                 // Pointer to raw point data
  const AttributeSchema* schema;       // Reference to the attribute schema

  Point() : data(nullptr), schema(nullptr), own_data(false) {}
  Point(AttributeSchema* schema) : data(new unsigned char[schema->total_point_size]), schema(schema), own_data(true) {  memset(data, 0, schema->total_point_size); }
  Point(unsigned char* ptr, const AttributeSchema* schema) : data(ptr), schema(schema), own_data(false) {}
  ~Point() {if (own_data && data) { delete[] data; data = nullptr; }; }

  Point(const Point& other) : data(other.own_data ? new unsigned char[other.schema->total_point_size] : other.data), schema(other.schema), own_data(other.own_data)
  {
    if (own_data) std::copy(other.data, other.data + other.schema->total_point_size, data);
  }

  Point& operator=(const Point& other)
  {
    if (this != &other)
    {
      if (own_data && data)
      {
        delete[] data;
        data = nullptr;
      }

      schema = other.schema;
      own_data = other.own_data;
      data = other.own_data ? new unsigned char[other.schema->total_point_size] : other.data;
      if (own_data) std::copy(other.data, other.data + other.schema->total_point_size, data);
    }
    return *this;
  }

  // Move constructor
  Point(Point&& other) noexcept : data(other.data), schema(other.schema), own_data(other.own_data)
  {
    other.data = nullptr;
    other.schema = nullptr;
    other.own_data = false;
  }

  // Move assignment operator
  Point& operator=(Point&& other) noexcept
  {
    if (this != &other)
    {
      if (own_data && data)
      {
        delete[] data;
        data = nullptr;
      }

      // Steal resources
      data = other.data;
      schema = other.schema;
      own_data = other.own_data;

      // Nullify the other object
      other.data = nullptr;
      other.schema = nullptr;
      other.own_data = false;
    }
    return *this;
  }

  void set_schema(AttributeSchema* schema)
  {
    this->schema = schema;
    if (own_data && data) delete[] data;
    data = nullptr;
  }

  double get_x() const {
    const auto& attr = schema->attributes[0];
    unsigned int X = *((unsigned int*)(data + attr.offset));
    return attr.scale_factor * X + attr.value_offset;
  }
  double get_y() const {
    const auto& attr = schema->attributes[1];
    unsigned int Y = *((unsigned int*)(data + attr.offset));
    return attr.scale_factor * Y + attr.value_offset;
  }
  double get_z() const {
    const auto& attr = schema->attributes[2];
    unsigned int Z = *((unsigned int*)(data + attr.offset));
    return attr.scale_factor * Z + attr.value_offset;
  }
  bool get_withheld() const {
    const auto& attr = schema->attributes[3];
    unsigned int flag = *((unsigned int*)(data + attr.offset));
    return ((flag & (1 << 0)) == 0) ? false : true;
  }
  void set_x(double value) {
    const auto& attr = schema->attributes[0];
    unsigned char* pointer = data + attr.offset;
    double scaled_value = (value - attr.value_offset) / attr.scale_factor;
    *reinterpret_cast<unsigned int*>(pointer) = static_cast<unsigned int>(scaled_value);
  }
  void set_y(double value) {
    const auto& attr = schema->attributes[1];
    unsigned char* pointer = data + attr.offset;
    double scaled_value = (value - attr.value_offset) / attr.scale_factor;
    *reinterpret_cast<unsigned int*>(pointer) = static_cast<unsigned int>(scaled_value);
  }
  void set_z(double value) {
    const auto& attr = schema->attributes[2];
    unsigned char* pointer = data + attr.offset;
    double scaled_value = (value - attr.value_offset) / attr.scale_factor;
    *reinterpret_cast<unsigned int*>(pointer) = static_cast<unsigned int>(scaled_value);
  }
  void set_withheld(bool value) {
    auto& attr = schema->attributes[3];
    unsigned int& flag = *((unsigned int*)(data + attr.offset));
    if (value) {
      flag |= (1 << 0); // Set the 0th bit to 1
    } else {
      flag &= ~(1 << 0); // Clear the 0th bit to 0
    }
  }
};

class AttributeReader
{
public:
  AttributeReader() : accessor(nullptr), attribute(nullptr), init(false) {}
  AttributeReader(const std::string& name);
  AttributeReader(const std::string& name, const AttributeSchema* schema);
  double operator()(Point* point);

protected:
  std::function<double(const Point*)> accessor; // Cached accessor function
  std::string name;
  bool init;
  const Attribute* attribute;                   // Cached attribute function
};

class AttributeWriter
{
public:
  AttributeWriter() : accessor(nullptr), attribute(nullptr), init(false) {}
  AttributeWriter(const std::string& name);
  AttributeWriter(const std::string& name, AttributeSchema* schema);
  void operator()(Point* point, double value);

protected:
  std::function<void(Point*, double)> accessor;  // Cached accessor function for setting value
  std::string name;
  bool init;
  const Attribute* attribute;  // Cached attribute function
};


class Header
{
public:
  double max_x;
  double min_x;
  double max_y;
  double min_y;
  double max_z;
  double min_z;
  uint64_t number_of_point_records;
  AttributeSchema schema;
  CRS crs;

  void add_attribute(const std::string& name, AttributeType type, double scale_factor = 1.0, double value_offset = 0.0)
  {
    schema.add_attribute(name, type, scale_factor, value_offset);
  }
};



#endif
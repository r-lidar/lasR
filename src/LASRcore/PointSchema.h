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
  {"Z", {"Z", "z"}},
  {"Intensity", {"Intensity", "intensity", "i"}},
  {"ReturnNumber", {"return", "Return", "ReturnNumber", "return_number", "r"}},
  {"NumberOfReturns", {"NumberOfReturns", "NumberReturns", "numberofreturns", "n"}},
  {"Classification", {"Classification", "classification", "class", "c"}},
  {"gpstime", {"gpstime", "gps_time", "GPStime", "t", "time", "gps"}},
  //s
  //k
  //w
  {"UserData", {"UserData", "userdata", "user_data", "ud", "u"}},
  {"PointSourceID", {"PointSourceID", "point_source", "point_source_id", "pointsourceid", "psid", "p"}},
  //e
  //d
  {"ScanAngle", {"angle", "Angle", "ScanAngle", "ScanAngleRank", "scan_angle", "a"}},
  {"R", {"R", "Red", "red"}},
  {"G", {"G", "Green", "green"}},
  {"B", {"B", "Blue", "blue"}},
  {"NIR", {"N", "NIR", "nir"}},
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
  Attribute(const std::string& name, AttributeType type, double scale_factor = 1.0, double value_offset = 0.0, const std::string& description = "");
  void dump(bool verbose = false) const;
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

  const Attribute* find_attribute(const std::string&) const;
  void add_attribute(const Attribute& attribute);
  void add_attribute(const std::string& name, AttributeType type, double scale_factor = 1.0, double value_offset = 0.0, const std::string& description = "");
  void dump(bool verbose = false) const;
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
  inline int get_X() const {
    const auto& attr = schema->attributes[0];
    int X = *((int*)(data + attr.offset));
    return X;
  }
  inline int get_Y() const {
    const auto& attr = schema->attributes[1];
    int Y = *((int*)(data + attr.offset));
    return Y;
  }
  inline int get_Z() const {
    const auto& attr = schema->attributes[2];
    int Z = *((int*)(data + attr.offset));
    return Z;
  }
  inline double get_x() const {
    const auto& attr = schema->attributes[0];
    int X = *((int*)(data + attr.offset));
    return attr.scale_factor * X + attr.value_offset;
  }
  inline double get_y() const {
    const auto& attr = schema->attributes[1];
    int Y = *((int*)(data + attr.offset));
    return attr.scale_factor * Y + attr.value_offset;
  }
  inline double get_z() const {
    const auto& attr = schema->attributes[2];
    int Z = *((int*)(data + attr.offset));
    return attr.scale_factor * Z + attr.value_offset;
  }
  inline bool get_deleted() const {
    const auto& attr = schema->attributes[3];
    unsigned int flag = *((unsigned int*)(data + attr.offset));
    return ((flag & (1 << 0)) == 0) ? false : true;
  }
  inline void set_X(int value) {
    const auto& attr = schema->attributes[0];
    unsigned char* pointer = data + attr.offset;
    *reinterpret_cast<int*>(pointer) = static_cast< int>(value);
  }
  inline void set_Y(int value) {
    const auto& attr = schema->attributes[1];
    unsigned char* pointer = data + attr.offset;
    *reinterpret_cast<int*>(pointer) = static_cast< int>(value);
  }
  inline void set_Z(int value) {
    const auto& attr = schema->attributes[2];
    unsigned char* pointer = data + attr.offset;
    *reinterpret_cast<int*>(pointer) = static_cast< int>(value);
  }
  inline void set_x(double value) {
    const auto& attr = schema->attributes[0];
    unsigned char* pointer = data + attr.offset;
    double scaled_value = (value - attr.value_offset) / attr.scale_factor;
    *reinterpret_cast<int*>(pointer) = static_cast<int>(scaled_value);
  }
  inline void set_y(double value) {
    const auto& attr = schema->attributes[1];
    unsigned char* pointer = data + attr.offset;
    double scaled_value = (value - attr.value_offset) / attr.scale_factor;
    *reinterpret_cast<int*>(pointer) = static_cast< int>(scaled_value);
  }
  inline void set_z(double value) {
    const auto& attr = schema->attributes[2];
    unsigned char* pointer = data + attr.offset;
    double scaled_value = (value - attr.value_offset) / attr.scale_factor;
    *reinterpret_cast<int*>(pointer) = static_cast<int>(scaled_value);
  }
  inline void set_deleted(bool value = true) {
    auto& attr = schema->attributes[3];
    unsigned int& flag = *((unsigned int*)(data + attr.offset));
    if (value) {
      flag |= (1 << 0); // Set the 0th bit to 1
    } else {
      flag &= ~(1 << 0); // Clear the 0th bit to 0
    }
  }
};

class AttributeHandler
{
public:
  // Constructors
  AttributeHandler(const std::string& name);
  AttributeHandler(const std::string& name, AttributeSchema* schema);
  AttributeHandler(const AttributeHandler& other);

  // Destructor
  ~AttributeHandler() = default;

  // Overloaded operators for reading and writing
  double operator()(const Point* point) const;
  void operator()(Point* point, double value);

public:
  std::string name;
  const Attribute* attribute;
  bool init;
  std::function<double(const Point*)> readAccessor;
  std::function<void(Point*, double)> writeAccessor;

  void initialize(const Point* point);
};

class AttributeReader
{
public:
  AttributeReader() : accessor(nullptr), attribute(nullptr), init(false) {}
  AttributeReader(const std::string& name);
  AttributeReader(const std::string& name, const AttributeSchema* schema);
  bool exist() { return attribute != nullptr; };
  double operator()(const Point* point);

  std::string name;
  const Attribute* attribute;                   // Cached attribute schema

protected:
  std::function<double(const Point*)> accessor; // Cached accessor function
  bool init;
};

class AttributeWriter
{
public:
  AttributeWriter() : accessor(nullptr), attribute(nullptr), init(false) {}
  AttributeWriter(const std::string& name);
  AttributeWriter(const std::string& name, AttributeSchema* schema);
  bool exist() { return attribute != nullptr; };
  void operator()(Point* point, double value);

  std::string name;
  const Attribute* attribute;  // Cached attribute function

protected:
  std::function<void(Point*, double)> accessor;  // Cached accessor function for setting value
  bool init;
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

  void add_attribute(const Attribute& attr)
  {
    schema.add_attribute(attr);
  }
};



#endif
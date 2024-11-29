#ifndef POINTSCHEMA_H
#define POINTSCHEMA_H

#include "CRS.h"
#include "print.h"

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <set>
#include <unordered_map>
#include <algorithm>

static const std::set<std::string> lascoreattributes = {
  "X", "Y", "Z", "Intensity", "ReturnNumber",
  "NumberOfReturns", "ScanDirectionFlag", "EdgeOfFlightLine",
  "Classification", "UserData", "ScanAngleRank", "PointSourceID",
  "gpstime", "ScannerChannel", "ScanAngle", "R", "G", "B", "flags"
};

static const std::unordered_map<std::string, std::vector<std::string>> attribute_map = {
  {"X", {"X", "x"}},
  {"Y", {"Y", "y"}},
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
  NOTYPE = 0,
  UINT8 = 1,
  INT8 = 2,
  UINT16 = 3,
  INT16 = 4,
  UINT32 = 5,
  INT32 = 6,
  UINT64 = 7,
  INT64 = 8,
  FLOAT = 9,
  DOUBLE = 10
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
  AttributeSchema() { total_point_size = 0; attributes.reserve(100); }
  std::vector<Attribute> attributes;
  size_t total_point_size;

  const Attribute* find_attribute(const std::string& s) const;
  bool has_attribute(const std::string& s) const { return find_attribute(s) != nullptr; };
  int get_attribute_index(const std::string& s) const;
  void add_attribute(const Attribute& attribute);
  void add_attribute(const std::string& name, AttributeType type, double scale_factor = 1.0, double value_offset = 0.0, const std::string& description = "");
  void dump(bool verbose = false) const;
};

struct Point
{
  bool own_data;
  unsigned char* data;                 // Pointer to raw point data
  const AttributeSchema* schema;       // Reference to the attribute schema

  Point() : own_data(false), data(nullptr), schema(nullptr) {}
  Point(AttributeSchema* schema) : own_data(true), data(new unsigned char[schema->total_point_size]), schema(schema) { zero(); }
  Point(unsigned char* ptr, const AttributeSchema* schema) : own_data(false), data(ptr), schema(schema) {}
  ~Point() {if (own_data && data) { delete[] data; data = nullptr; }; }
  void dump() const { print("%.2lf %.2lf %.2lf\n", get_x(), get_y(), get_z()); };

  Point(const Point& other) : own_data(other.own_data), data(other.own_data ? new unsigned char[other.schema->total_point_size] : other.data), schema(other.schema)
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
  Point(Point&& other) noexcept : own_data(other.own_data), data(other.data), schema(other.schema)
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
  inline bool get_flag(int i) const {
    const auto& attr = schema->attributes[3];
    unsigned int flag = *((unsigned int*)(data + attr.offset));
    return ((flag & (1 << i)) == 0) ? false : true;
  }
  inline bool get_deleted() const {
    return get_flag(0);
  }
  inline bool get_buffered() const {
    return get_flag(1);
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
  inline double get_attribute_as_double(int index)
  {
    const auto& attr = schema->attributes[index];
    unsigned char* pointer = data + attr.offset;
    double cast_value = 0;
    switch (attr.type) {
    case UINT8: cast_value = static_cast<double>(*reinterpret_cast<const uint8_t*>(pointer)); break;
    case INT8: cast_value = static_cast<double>(*reinterpret_cast<const int8_t*>(pointer)); break;
    case UINT16: cast_value = static_cast<double>(*reinterpret_cast<const uint16_t*>(pointer)); break;
    case INT16: cast_value = static_cast<double>(*reinterpret_cast<const int16_t*>(pointer)); break;
    case UINT32: cast_value = static_cast<double>(*reinterpret_cast<const uint32_t*>(pointer)); break;
    case INT32: cast_value = static_cast<double>(*reinterpret_cast<const int32_t*>(pointer)); break;
    case UINT64: cast_value = static_cast<double>(*reinterpret_cast<const uint64_t*>(pointer)); break;
    case INT64: cast_value = static_cast<double>(*reinterpret_cast<const int64_t*>(pointer)); break;
    case FLOAT: cast_value = static_cast<double>(*reinterpret_cast<const float*>(pointer)); break;
    case DOUBLE: cast_value = static_cast<double>(*reinterpret_cast<const double*>(pointer)); break;
    default: return 0.0;
    }
    return attr.value_offset + attr.scale_factor * cast_value;
  }
  inline void set_flag(int i, bool value = true) {
    auto& attr = schema->attributes[3];
    unsigned int& flag = *((unsigned int*)(data + attr.offset));
    if (value) {
      flag |= (1 << i); // Set the ith bit to 1
    } else {
      flag &= ~(1 << i); // Clear the ith bit to 0
    }
  }
  inline void set_deleted(bool value = true) {
    set_flag(0, value);
  }
  inline void set_buffered(bool value = true) {
    set_flag(1, value);
  }
  inline void zero() {
    memset(data, 0, schema->total_point_size);
  }
  inline bool inside_buffer(const double min_x, const double min_y, const double max_x, const double max_y, const bool circle = false)
  {
    double x = get_x();
    if (x < min_x || x > max_x) return true;
    double y = get_y();
    if (y < min_y || y > max_y) return true;

    if (circle)
    {
      double r2 = (max_x-min_x)/2;
      double center_x = (max_x+min_x)/2;
      double center_y = (max_y+min_y)/2;
      double dx = center_x - get_x();
      double dy = center_y - get_y();
      return ((dx*dx+dy*dy) > r2*r2);
    }

    return false;
  }
};

class AttributeHandler {
public:
  AttributeHandler(double default_value = 0);
  AttributeHandler(const std::string& name, double default_value = 0);
  AttributeHandler(const std::string& name, AttributeSchema* schema, double default_value = 0);
  ~AttributeHandler() = default;

  // Overloaded operators for reading and writing
  double operator()(const Point* point);
  void operator()(Point* point, double value);
  bool exist() { return attribute != nullptr; }
  void reset() { init = false; attribute = nullptr; };

protected:
  std::string name;
  const Attribute* attribute;
  bool init;
  double default_value;

  double read(const Point* point);
  void write(Point* point, double value);
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
  double x_scale_factor, y_scale_factor, z_scale_factor;
  double x_offset, y_offset, z_offset;
  bool adjusted_standard_gps_time;
  uint64_t number_of_point_records;
  AttributeSchema schema;
  CRS crs;

  void add_attribute(const Attribute& attr)
  {
    schema.add_attribute(attr);
  }
};



#endif
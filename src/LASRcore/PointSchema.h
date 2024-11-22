#ifndef HEADER_H
#define HEADER_H

#include "CRS.h"

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#include <iostream>
#include <vector>
#include <string>

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
  AttributeReader() : accessor(nullptr), attribute(nullptr) {}

  AttributeReader(const std::string& name, const AttributeSchema* schema) : accessor(nullptr), attribute(nullptr)
  {
    for (const auto& attr : schema->attributes)
    {
      if (attr.name == name)
      {
        attribute = &attr;
        if (attribute->type > 8) throw std::runtime_error("Unsupported type for attribute.");
        break;
      }
    }

    accessor = [this](const Point* point)
    {
      if (this->attribute == nullptr)
      {
        return 0.0;
      }

      unsigned char* pointer = point->data + attribute->offset;
      double cast_value = 0;

      // Cast value based on the type
      switch (attribute->type)
      {
      case 0: cast_value = (double)(*reinterpret_cast<uint8_t*>(pointer)); break;
      case 1: cast_value = (double)(*reinterpret_cast<int8_t*>(pointer)); break;
      case 2: cast_value = (double)(*reinterpret_cast<uint16_t*>(pointer)); break;
      case 3: cast_value = (double)(*reinterpret_cast<int16_t*>(pointer)); break;
      case 4: cast_value = (double)(*reinterpret_cast<uint32_t*>(pointer)); break;
      case 5: cast_value = (double)(*reinterpret_cast<int32_t*>(pointer)); break;
      case 6: cast_value = (double)(*reinterpret_cast<uint64_t*>(pointer)); break;
      case 7: cast_value = (double)(*reinterpret_cast<int64_t*>(pointer)); break;
      case 8: cast_value = (double)(*reinterpret_cast<float*>(pointer)); break;
      case 9: cast_value = (double)(*reinterpret_cast<double*>(pointer)); break;
      }

      return attribute->value_offset + attribute->scale_factor * cast_value;
    };
  }

  double operator()(Point* point)
  {
    if (!accessor) {
      throw std::runtime_error("Accessor not initialized.");
    }
    return accessor(point);
  }

private:
  std::function<double(const Point*)> accessor; // Cached accessor function
  const Attribute* attribute;                   // Cached attribute function
};

class AttributeWriter
{
public:
  AttributeWriter() : accessor(nullptr), attribute(nullptr) {}

  AttributeWriter(const std::string& name, AttributeSchema* schema) : accessor(nullptr), attribute(nullptr)
  {
    for (auto& attr : schema->attributes)
    {
      if (attr.name == name)
      {
        attribute = &attr;
        if (attribute->type > 8) throw std::runtime_error("Unsupported type for attribute.");
        break;
      }
    }

    accessor = [this](Point* point, double value)
    {
      if (this->attribute == nullptr)
      {
        return;
      }

      unsigned char* pointer = point->data + attribute->offset;

      // Scale the value and apply the offset
      double scaled_value = (value - attribute->value_offset) / attribute->scale_factor;

      // Write the value based on the attribute's type
      switch (attribute->type)
      {
      case 0: *reinterpret_cast<uint8_t*>(pointer) = static_cast<uint8_t>(scaled_value); break;
      case 1: *reinterpret_cast<int8_t*>(pointer) = static_cast<int8_t>(scaled_value); break;
      case 2: *reinterpret_cast<uint16_t*>(pointer) = static_cast<uint16_t>(scaled_value); break;
      case 3: *reinterpret_cast<int16_t*>(pointer) = static_cast<int16_t>(scaled_value); break;
      case 4: *reinterpret_cast<uint32_t*>(pointer) = static_cast<uint32_t>(scaled_value); break;
      case 5: *reinterpret_cast<int32_t*>(pointer) = static_cast<int32_t>(scaled_value); break;
      case 6: *reinterpret_cast<uint64_t*>(pointer) = static_cast<uint64_t>(scaled_value); break;
      case 7: *reinterpret_cast<int64_t*>(pointer) = static_cast<int64_t>(scaled_value); break;
      case 8: *reinterpret_cast<float*>(pointer) = scaled_value; break;
      case 9: *reinterpret_cast<double*>(pointer) = scaled_value; break;
      }
    };
  }

  void operator()(Point* point, double value)
  {
    if (!accessor) {
      throw std::runtime_error("Accessor not initialized.");
    }
    accessor(point, value);
  }

private:
  std::function<void(Point*, double)> accessor;  // Cached accessor function for setting value
  Attribute* attribute;                          // Cached attribute function
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
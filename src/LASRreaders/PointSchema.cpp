#include "PointSchema.h"

#include <stdexcept> // std::runtime_error
#include <algorithm> // std::clamp
#include <cmath> // std::round
#include <limits> // std::numeric_limits

Attribute::Attribute(const std::string& name, AttributeType type, double scale_factor, double value_offset, const std::string& description)
{
  this->name = name;
  this->type = type;
  this->scale_factor = scale_factor;
  this->value_offset = value_offset;
  this->description = description;
  this->bit_pos = 0;
  switch (type)
  {
  case BIT:
  case UINT8:
  case INT8:
    size = 1; break;
  case UINT16:
  case INT16:
    size = 2; break;
  case UINT32:
  case INT32:
  case FLOAT:
    size = 4; break;
  case UINT64:
  case INT64:
  case DOUBLE:
    size = 8; break;
  default:
    size = 0;
  }
}

void Attribute::dump(bool verbose) const
{
  if (verbose)
    print(" Name: %-17s | Address offset: %-2zu | Size: %-1zu | Type: %-6s | Scale Factor: %-5.3f | Value Offset: %-5.3f\n", name.c_str(), offset, size, attributeTypeToString(), scale_factor, value_offset);
  else
    print(" Name: %-17s | %-6s | Desc: %s\n", name.c_str(), attributeTypeToString(), description.c_str());
}

void AttributeSchema::add_attribute(const Attribute& attribute)
{
  if (attributes.size() >= attributes.capacity())
  {
    // Avoid reallocation and pointer invalidation in other objects with pointer to the attributes
    std::runtime_error("Too many attributes for this point cloud");
  }
  attributes.push_back(attribute);

  // Regular attributes
  if (attribute.type != AttributeType::BIT)
  {
    n_consecutive_bits = 0;
    attributes.back().offset = total_point_size;
    total_point_size += attribute.size;
    return;
  }

  // Single bit attribute
  if (n_consecutive_bits == 8) n_consecutive_bits = 0;
  if (n_consecutive_bits == 0) n_consecutive_bits++;
  if (n_consecutive_bits == 1) total_point_size++;
  attributes.back().offset = total_point_size-1;
  attributes.back().bit_pos = n_consecutive_bits-1;
  n_consecutive_bits++;
}

void AttributeSchema::add_attribute(const std::string& name, AttributeType type, double scale_factor, double value_offset, const std::string& description)
{
  Attribute attr(name, type, scale_factor, value_offset, description);
  add_attribute(attr);
}

void AttributeSchema::remove_attribute(const std::string& attribute_name)
{
  int idx = get_attribute_index(attribute_name);
  if (idx < 0) return;

  const Attribute& attr_to_remove = attributes[idx];
  size_t size_to_remove = attr_to_remove.size;

  // Erase the attribute from the vector
  attributes.erase(attributes.begin() + idx);

  // Adjust offsets of subsequent attributes
  for (size_t i = idx; i < attributes.size(); ++i)
  {
    attributes[i].offset -= size_to_remove;
  }

  // Update total point size
  total_point_size -= size_to_remove;
}

const Attribute* AttributeSchema::find_attribute(const std::string& name) const
{
  for (auto& attribute : attributes)
  {
    if (attribute.name == name)
      return &attribute;
  }

  return nullptr;
}

int AttributeSchema::get_attribute_index(const std::string& name) const
{
  int i = 0;
  for (auto& attribute : attributes)
  {
    if (attribute.name == name)
      return i;

    i++;
  }

  return -1;
}

void AttributeSchema::dump(bool verbose) const
{
  print("%d attributes | %d bytes per points\n", num_attributes(), total_point_size);
  for (const auto& attr : attributes) attr.dump(verbose);
}

AttributeAccessor::AttributeAccessor(double default_value) : name(""), attribute(nullptr), init(false), default_value(default_value) {}
AttributeAccessor::AttributeAccessor(const std::string& name, double default_value) : name(name), attribute(nullptr), init(false), default_value(default_value) {}
AttributeAccessor::AttributeAccessor(const std::string& name, AttributeSchema* schema, double default_value) : name(name), attribute(schema->find_attribute(name)), init(true), default_value(default_value)
{
  if (!attribute) throw std::runtime_error("Attribute not found in schema");
}

double AttributeAccessor::read(const Point* point)
{
  if (!init)
  {
    attribute = point->schema->find_attribute(name);
    init = true;
  }
  if (!attribute) return default_value;

  unsigned char* pointer = point->data + attribute->offset;
  double cast_value = 0;
  switch (attribute->type) {
  case BIT: cast_value = static_cast<double>(((*pointer >> attribute->bit_pos) & 1)); break;
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
  default: return default_value;
  }
  return attribute->value_offset + attribute->scale_factor * cast_value;
}

void AttributeAccessor::write(Point* point, double value)
{
  if (!init)
  {
    attribute = point->schema->find_attribute(name);
    init = true;
  }
  if (!attribute) return;

  unsigned char* pointer = point->data + attribute->offset;
  double scaled_value = (value - attribute->value_offset) / attribute->scale_factor;

  switch (attribute->type) {
  case BIT:    *reinterpret_cast<uint8_t*>(pointer) = (*reinterpret_cast<uint8_t*>(pointer) & ~(1 << attribute->bit_pos)) | ((value != 0.0) << attribute->bit_pos); break;
  case UINT8:  *reinterpret_cast<uint8_t*>(pointer)  = static_cast<uint8_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint8_t>::lowest()), static_cast<double>(std::numeric_limits<uint8_t>::max()))); break;
  case INT8:   *reinterpret_cast<int8_t*>(pointer)   = static_cast<int8_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int8_t>::lowest()), static_cast<double>(std::numeric_limits<int8_t>::max()))); break;
  case UINT16: *reinterpret_cast<uint16_t*>(pointer) = static_cast<uint16_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint16_t>::lowest()), static_cast<double>(std::numeric_limits<uint16_t>::max()))); break;
  case INT16:  *reinterpret_cast<int16_t*>(pointer)  = static_cast<int16_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int16_t>::lowest()), static_cast<double>(std::numeric_limits<int16_t>::max()))); break;
  case UINT32: *reinterpret_cast<uint32_t*>(pointer) = static_cast<uint32_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint32_t>::lowest()), static_cast<double>(std::numeric_limits<uint32_t>::max()))); break;
  case INT32:  *reinterpret_cast<int32_t*>(pointer)  = static_cast<int32_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int32_t>::lowest()), static_cast<double>(std::numeric_limits<int32_t>::max()))); break;
  case UINT64: *reinterpret_cast<uint64_t*>(pointer) = static_cast<uint64_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint64_t>::lowest()), static_cast<double>(std::numeric_limits<uint64_t>::max()))); break;
  case INT64:  *reinterpret_cast<int64_t*>(pointer)  = static_cast<int64_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int64_t>::lowest()), static_cast<double>(std::numeric_limits<int64_t>::max()))); break;
  case FLOAT:  *reinterpret_cast<float*>(pointer)    = static_cast<float>(std::clamp(scaled_value, static_cast<double>(std::numeric_limits<float>::lowest()), static_cast<double>(std::numeric_limits<float>::max())));  break;
  case DOUBLE: *reinterpret_cast<double*>(pointer)   = scaled_value; break;
  default: return;
  }
}

double AttributeAccessor::operator()(const Point* point)
{
  return read(point);
}

void AttributeAccessor::operator()(Point* point, double value)
{
  write(point, value);
}
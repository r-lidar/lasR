#include "PointSchema.h"

Attribute::Attribute(const std::string& name, AttributeType type, double scale_factor, double value_offset, const std::string& description)
{
  this->name = name;
  this->type = type;
  this->scale_factor = scale_factor;
  this->value_offset = value_offset;
  this->description = description;
  switch (type)
  {
  case AttributeType::UINT8:
  case AttributeType::INT8:
    size = 1; break;
  case AttributeType::UINT16:
  case AttributeType::INT16:
    size = 2; break;
  case AttributeType::UINT32:
  case AttributeType::INT32:
  case AttributeType::FLOAT:
    size = 4; break;
  case AttributeType::UINT64:
  case AttributeType::INT64:
  case AttributeType::DOUBLE:
    size = 8; break;
  default:
    size = 0;
  }
}

void Attribute::dump(bool verbose) const
{
  if (verbose)
    print("Attribute: %-15s | Address offset: %-2zu | Size: %-1zu | Type: %-6s | Scale Factor: %-5.2f | Value Offset: %-5.2f\n", name.c_str(), offset, size, attributeTypeToString(), scale_factor, value_offset);
  else
    print("Name: %-15s | %-6s | Desc: %s\n", name.c_str(), attributeTypeToString(), description.c_str());
}

void AttributeSchema::add_attribute(const Attribute& attribute)
{
  if (attributes.size() >= attributes.capacity())
  {
    // Avoid reallocation and pointer invalidation in other objects with pointer to the attributes
    std::runtime_error("Too many attributes for this point cloud");
  }
  attributes.push_back(attribute);
  attributes.back().offset = total_point_size;
  total_point_size += attribute.size;
}

void AttributeSchema::add_attribute(const std::string& name, AttributeType type, double scale_factor, double value_offset, const std::string& description)
{
  Attribute attr(name, type, scale_factor, value_offset, description);
  add_attribute(attr);
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
  case AttributeType::UINT8: cast_value = static_cast<double>(*reinterpret_cast<const uint8_t*>(pointer)); break;
  case AttributeType::INT8: cast_value = static_cast<double>(*reinterpret_cast<const int8_t*>(pointer)); break;
  case AttributeType::UINT16: cast_value = static_cast<double>(*reinterpret_cast<const uint16_t*>(pointer)); break;
  case AttributeType::INT16: cast_value = static_cast<double>(*reinterpret_cast<const int16_t*>(pointer)); break;
  case AttributeType::UINT32: cast_value = static_cast<double>(*reinterpret_cast<const uint32_t*>(pointer)); break;
  case AttributeType::INT32: cast_value = static_cast<double>(*reinterpret_cast<const int32_t*>(pointer)); break;
  case AttributeType::UINT64: cast_value = static_cast<double>(*reinterpret_cast<const uint64_t*>(pointer)); break;
  case AttributeType::INT64: cast_value = static_cast<double>(*reinterpret_cast<const int64_t*>(pointer)); break;
  case AttributeType::FLOAT: cast_value = static_cast<double>(*reinterpret_cast<const float*>(pointer)); break;
  case AttributeType::DOUBLE: cast_value = static_cast<double>(*reinterpret_cast<const double*>(pointer)); break;
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
  case AttributeType::UINT8:  *reinterpret_cast<uint8_t*>(pointer)  = static_cast<uint8_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint8_t>::lowest()), static_cast<double>(std::numeric_limits<uint8_t>::max()))); break;
  case AttributeType::INT8:   *reinterpret_cast<int8_t*>(pointer)   = static_cast<int8_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int8_t>::lowest()), static_cast<double>(std::numeric_limits<int8_t>::max()))); break;
  case AttributeType::UINT16: *reinterpret_cast<uint16_t*>(pointer) = static_cast<uint16_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint16_t>::lowest()), static_cast<double>(std::numeric_limits<uint16_t>::max()))); break;
  case AttributeType::INT16:  *reinterpret_cast<int16_t*>(pointer)  = static_cast<int16_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int16_t>::lowest()), static_cast<double>(std::numeric_limits<int16_t>::max()))); break;
  case AttributeType::UINT32: *reinterpret_cast<uint32_t*>(pointer) = static_cast<uint32_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint32_t>::lowest()), static_cast<double>(std::numeric_limits<uint32_t>::max()))); break;
  case AttributeType::INT32:  *reinterpret_cast<int32_t*>(pointer)  = static_cast<int32_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int32_t>::lowest()), static_cast<double>(std::numeric_limits<int32_t>::max()))); break;
  case AttributeType::UINT64: *reinterpret_cast<uint64_t*>(pointer) = static_cast<uint64_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<uint64_t>::lowest()), static_cast<double>(std::numeric_limits<uint64_t>::max()))); break;
  case AttributeType::INT64:  *reinterpret_cast<int64_t*>(pointer)  = static_cast<int64_t>(std::clamp(std::round(scaled_value), static_cast<double>(std::numeric_limits<int64_t>::lowest()), static_cast<double>(std::numeric_limits<int64_t>::max()))); break;
  case AttributeType::FLOAT:  *reinterpret_cast<float*>(pointer)    = static_cast<float>(std::clamp(scaled_value, static_cast<double>(std::numeric_limits<float>::lowest()), static_cast<double>(std::numeric_limits<float>::max())));  break;
  case AttributeType::DOUBLE: *reinterpret_cast<double*>(pointer)   = scaled_value; break;
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
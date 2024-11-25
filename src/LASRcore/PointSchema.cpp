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
    size = 0;
  }
}

void Attribute::dump(bool verbose) const
{
  if (verbose)
    printf("Attribute: %-15s | Offset: %-2zu | Size: %-1zu | Type: %-1d | Scale Factor: %-5.2f | Offset: %-5.2f\n", name.c_str(), offset, size, type, scale_factor, value_offset);
  else
    printf("Attribute: %-15s | Description: %s\n", name.c_str(), description.c_str());
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

AttributeHandler::AttributeHandler() : name(""), attribute(nullptr), init(false) {}
AttributeHandler::AttributeHandler(const std::string& name) : name(name), attribute(nullptr), init(false) {}
AttributeHandler::AttributeHandler(const std::string& name, AttributeSchema* schema) : name(name), attribute(schema->find_attribute(name)), init(true)
{
  if (!attribute) throw std::runtime_error("Attribute not found in schema");
}

double AttributeHandler::read(const Point* point)
{
  if (!init)
  {
    attribute = point->schema->find_attribute(name);
    init = true;
  }
  if (!attribute) return 0.0;

  unsigned char* pointer = point->data + attribute->offset;
  double cast_value = 0;
  switch (attribute->type) {
  case 0: cast_value = static_cast<double>(*reinterpret_cast<const uint8_t*>(pointer)); break;
  case 1: cast_value = static_cast<double>(*reinterpret_cast<const int8_t*>(pointer)); break;
  case 2: cast_value = static_cast<double>(*reinterpret_cast<const uint16_t*>(pointer)); break;
  case 3: cast_value = static_cast<double>(*reinterpret_cast<const int16_t*>(pointer)); break;
  case 4: cast_value = static_cast<double>(*reinterpret_cast<const uint32_t*>(pointer)); break;
  case 5: cast_value = static_cast<double>(*reinterpret_cast<const int32_t*>(pointer)); break;
  case 6: cast_value = static_cast<double>(*reinterpret_cast<const uint64_t*>(pointer)); break;
  case 7: cast_value = static_cast<double>(*reinterpret_cast<const int64_t*>(pointer)); break;
  case 8: cast_value = static_cast<double>(*reinterpret_cast<const float*>(pointer)); break;
  case 9: cast_value = static_cast<double>(*reinterpret_cast<const double*>(pointer)); break;
  default: return 0.0;
  }
  return attribute->value_offset + attribute->scale_factor * cast_value;
}

void AttributeHandler::write(Point* point, double value)
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
  case 0: *reinterpret_cast<uint8_t*>(pointer) = static_cast<uint8_t>(scaled_value); break;
  case 1: *reinterpret_cast<int8_t*>(pointer) = static_cast<int8_t>(scaled_value); break;
  case 2: *reinterpret_cast<uint16_t*>(pointer) = static_cast<uint16_t>(scaled_value); break;
  case 3: *reinterpret_cast<int16_t*>(pointer) = static_cast<int16_t>(scaled_value); break;
  case 4: *reinterpret_cast<uint32_t*>(pointer) = static_cast<uint32_t>(scaled_value); break;
  case 5: *reinterpret_cast<int32_t*>(pointer) = static_cast<int32_t>(scaled_value); break;
  case 6: *reinterpret_cast<uint64_t*>(pointer) = static_cast<uint64_t>(scaled_value); break;
  case 7: *reinterpret_cast<int64_t*>(pointer) = static_cast<int64_t>(scaled_value); break;
  case 8: *reinterpret_cast<float*>(pointer) = static_cast<float>(scaled_value); break;
  case 9: *reinterpret_cast<double*>(pointer) = scaled_value; break;
  default: return;
  }
}

double AttributeHandler::operator()(const Point* point)
{
  return read(point);
}

void AttributeHandler::operator()(Point* point, double value)
{
  write(point, value);
}
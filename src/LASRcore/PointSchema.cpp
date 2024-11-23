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

void AttributeSchema::add_attribute(const Attribute& attribute)
{
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
    {
      return &attribute;
    }
  }

  return nullptr;
}

AttributeReader::AttributeReader(const std::string& name) : attribute(nullptr), init(false), name(name)
{
  accessor = [this](const Point* point)
  {
    if (!this->init)
    {
      attribute = point->schema->find_attribute(this->name);
      init = true;
      this->init = true;
    }

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

AttributeReader::AttributeReader(const std::string& name, const AttributeSchema* schema) : accessor(nullptr), attribute(nullptr), name(name)
{
  attribute = schema->find_attribute(name);
  init = true;

  accessor = [this](const Point* point)
  {
    if (!this->init)
    {
      attribute = point->schema->find_attribute(this->name);
      this->init = true;
    }

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

double AttributeReader::operator()(Point* point)
{
    return accessor(point);
}


AttributeWriter::AttributeWriter(const std::string& name) : accessor(nullptr), attribute(nullptr), init(false), name(name)
{
  accessor = [this](Point* point, double value)
  {
    if (!this->init)
    {
      attribute = point->schema->find_attribute(this->name);
      this->init = true;
    }

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

AttributeWriter::AttributeWriter(const std::string& name, AttributeSchema* schema) : accessor(nullptr), attribute(nullptr)
{
  attribute = schema->find_attribute(name);
  init = true;

  accessor = [this](Point* point, double value)
  {
    if (!this->init)
    {
      for (const auto& attr : point->schema->attributes)
      {
        if (attr.name == this->name)
        {
          attribute = &attr;
          if (attribute->type > AttributeType::DOUBLE) throw std::runtime_error("Unsupported type for attribute.");
          break;
        }
      }

      this->init = true;
    }

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

void AttributeWriter::operator()(Point* point, double value)
{
  return accessor(point, value);
}

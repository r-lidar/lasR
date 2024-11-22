#include "PointSchema.h"

AttributeReader::AttributeReader(const std::string& name) : attribute(nullptr), init(false), name(name)
{
  accessor = [this](const Point* point)
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
  for (const auto& attr : schema->attributes)
  {
    if (attr.name == name)
    {
      attribute = &attr;
      if (attribute->type > AttributeType::DOUBLE) throw std::runtime_error("Unsupported type for attribute.");
      break;
    }
  }

  init = true;

  accessor = [this](const Point* point)
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

AttributeWriter::AttributeWriter(const std::string& name, AttributeSchema* schema) : accessor(nullptr), attribute(nullptr)
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

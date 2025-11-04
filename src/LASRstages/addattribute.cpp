#include "addattribute.h"

bool LASRaddattribute::process(PointCloud*& las)
{
  std::string standard_name = map_attribute(name);
  if (standard_name != name && las->header->schema.has_attribute(standard_name))
  {
    last_error = std::string("The attribute '") + name + "' is a reserved word interpreted as '" + standard_name + "'. This point cloud has already an attribute named '" + standard_name + "' thus adding this attribute is failing";
    return false;
  }

  Attribute attr(name, data_type, scale, offset, description);
  return las->add_attribute(attr);
}

bool LASRaddattribute::set_parameters(const nlohmann::json& stage)
{
  std::string type = stage.at("data_type");
  name = stage.at("name");
  description = stage.at("description");
  scale = stage.value("scale", 1.0);
  offset = stage.value("offset", 0.0);
  data_type = AttributeType::NOTYPE;

  if (type == "uchar")       data_type = AttributeType::UINT8;
  else if (type == "char")   data_type = AttributeType::INT8;
  else if (type == "ushort") data_type = AttributeType::UINT16;
  else if (type == "short")  data_type = AttributeType::INT16;
  else if (type == "uint")   data_type = AttributeType::UINT32;
  else if (type == "int")    data_type = AttributeType::INT32;
  else if (type == "uint64") data_type = AttributeType::UINT64;
  else if (type == "int64")  data_type = AttributeType::INT64;
  else if (type == "float")  data_type = AttributeType::FLOAT;
  else if (type == "double") data_type = AttributeType::DOUBLE;

  return true;
}

bool LASRremoveattributes::set_parameters(const nlohmann::json& stage)
{
  static const std::vector<std::string> forbidden = {"x", "X", "y", "Y", "z", "Z"};

  // Detect operation from JSON keys
  if (stage.contains("keep"))
  {
    op_type = AttributeOpType::Keep;
    names = stage.at("keep").get<std::vector<std::string>>();
  }
  else if (stage.contains("names"))
  {
    op_type = AttributeOpType::RemoveMultiple;
    names = stage.at("names").get<std::vector<std::string>>();
  }
  else if (stage.contains("name"))
  {
    op_type = AttributeOpType::RemoveSingle;
    name = stage.at("name").get<std::string>();
  }
  else
  {
    last_error = "No valid key found in JSON (expected 'name', 'names', or 'keep')";
    return false;
  }

  // Check forbidden attributes
  switch(op_type)
  {
  case AttributeOpType::RemoveSingle:
    if (std::find(forbidden.begin(), forbidden.end(), name) != forbidden.end())
    {
      last_error = "Removing point coordinates is not allowed";
      return false;
    }
    break;

  case AttributeOpType::RemoveMultiple:
    for (const auto& n : names)
    {
      if (std::find(forbidden.begin(), forbidden.end(), n) != forbidden.end())
      {
        last_error = "Removing point coordinates is not allowed";
        return false;
      }
    }
    break;
  case AttributeOpType::Keep:
    break;
  }

  return true;
}

bool LASRremoveattributes::process(PointCloud*& las)
{
  switch(op_type)
  {
  case AttributeOpType::RemoveSingle:
    return las->remove_attribute(name);

  case AttributeOpType::RemoveMultiple:
    return las->remove_attributes(names);

  case AttributeOpType::Keep:
    return las->keep_attributes(names);
  }

  return false; // should never reach here
}

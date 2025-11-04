#include "addattribute.h"

LASRaddattribute::LASRaddattribute()
{
}

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

LASRremoveattribute::LASRremoveattribute()
{
}

bool LASRremoveattribute::process(PointCloud*& las)
{
  return las->remove_attribute(name);
}

bool LASRremoveattribute::set_parameters(const nlohmann::json& stage)
{
  name = stage.at("name");
  if (name == "x" || name == "X" ||
      name == "y" || name == "Y" ||
      name == "z" || name == "Z")
  {
    last_error = "Removing point coordinates is not allowed";
    return false;
  }
  return true;
}

LASRremoveattributes::LASRremoveattributes()
{
}

bool LASRremoveattributes::process(PointCloud*& las)
{
  return las->remove_attributes(names);
}

bool LASRremoveattributes::set_parameters(const nlohmann::json& stage)
{
  names = stage.at("names").get<std::vector<std::string>>();

  static const std::vector<std::string> forbidden = {"x", "X", "y", "Y", "z", "Z"};

  for (const auto& name : names)
  {
    if (std::find(forbidden.begin(), forbidden.end(), name) != forbidden.end())
    {
      last_error = "Removing point coordinates is not allowed";
      return false;
    }
  }

  return true;
}

#include "addattribute.h"

LASRaddattribute::LASRaddattribute()
{
}

bool LASRaddattribute::process(LAS*& las)
{
  return las->add_attribute(data_type, name, description, scale, offset);
}

bool LASRaddattribute::set_parameters(const nlohmann::json& stage)
{
  std::string type = stage.at("data_type");
  name = stage.at("name");
  description = stage.at("description");
  scale = stage.value("scale", 1);
  offset = stage.value("offset", 0);
  data_type = 0;

  if (type == "uchar")       data_type = 1;
  else if (type == "char")   data_type = 2;
  else if (type == "ushort") data_type = 3;
  else if (type == "short")  data_type = 4;
  else if (type == "uint")   data_type = 5;
  else if (type == "int")    data_type = 6;
  else if (type == "uint64") data_type = 7;
  else if (type == "int64")  data_type = 8;
  else if (type == "float")  data_type = 9;
  else if (type == "double") data_type = 10;

  return true;
}
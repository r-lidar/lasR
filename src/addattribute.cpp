#include "addattribute.h"

LASRaddattribute::LASRaddattribute(std::string data_type, std::string name, std::string description, double scale, double offset)
{
  int type = 0;
  if (data_type == "uchar")       type = 1;
  else if (data_type == "char")   type = 2;
  else if (data_type == "ushort") type = 3;
  else if (data_type == "short")  type = 4;
  else if (data_type == "uint")   type = 5;
  else if (data_type == "int")    type = 6;
  else if (data_type == "uint64") type = 7;
  else if (data_type == "int64")  type = 8;
  else if (data_type == "float")  type = 9;
  else if (data_type == "double") type = 10;

  this->data_type = type;
  this->name = name;
  this->description = description;
  this->scale = scale;
  this->offset = offset;
};

bool LASRaddattribute::process(LAS*& las)
{
  return las->add_attribute(data_type, name, description, scale, offset);
};
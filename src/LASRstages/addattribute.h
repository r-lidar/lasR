#ifndef LASRADDATTR_H
#define LASRADDATTR_H

#include "Stage.h"

class LASRaddattribute: public Stage
{
public:
  LASRaddattribute() = default;
  bool process(PointCloud*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "add_attribute"; };

  // multi-threading
  LASRaddattribute* clone() const override { return new LASRaddattribute(*this); };

private:
  AttributeType data_type;
  double scale;
  double offset;
  std::string name;
  std::string description;
};

enum class AttributeOpType
{
  RemoveSingle,
  RemoveMultiple,
  Keep
};

class LASRremoveattributes: public Stage
{
public:
  LASRremoveattributes() = default;
  bool process(PointCloud*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "remove_attributes"; };

  // multi-threading
  LASRremoveattributes* clone() const override { return new LASRremoveattributes(*this); };

private:
  AttributeOpType op_type = AttributeOpType::RemoveSingle;
  std::string name;                 // for single remove
  std::vector<std::string> names;   // for multiple remove or keep
  std::string last_error;
};

#endif
#ifndef LASRADDATTR_H
#define LASRADDATTR_H

#include "Stage.h"

class LASRaddattribute: public Stage
{
public:
  LASRaddattribute();
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

class LASRremoveattribute: public Stage
{
public:
  LASRremoveattribute();
  bool process(PointCloud*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "remove_attribute"; };

  // multi-threading
  LASRremoveattribute* clone() const override { return new LASRremoveattribute(*this); };

private:
  std::string name;
};

class LASRremoveattributes: public Stage
{
public:
  LASRremoveattributes();
  bool process(PointCloud*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "remove_attributes"; };

  // multi-threading
  LASRremoveattributes* clone() const override { return new LASRremoveattributes(*this); };

private:
  std::vector<std::string> names;
};

#endif
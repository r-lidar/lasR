#ifndef LASRADDATTR_H
#define LASRADDATTR_H

#include "Stage.h"

class LASRaddattribute: public Stage
{
public:
  LASRaddattribute();
  bool process(LAS*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "add_extrabytes"; };

  // multi-threading
  LASRaddattribute* clone() const override { return new LASRaddattribute(*this); };

private:
  AttributeType data_type;
  double scale;
  double offset;
  std::string name;
  std::string description;
};

#endif
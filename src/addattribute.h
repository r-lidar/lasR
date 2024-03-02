#ifndef LASRADDATTR_H
#define LASRADDATTR_H

#include "lasralgorithm.h"

class LASRaddattribute: public LASRalgorithm
{
public:
  LASRaddattribute(std::string data_type, std::string name, std::string description, double scale = 1, double offset = 0);
  bool process(LAS*& las) override;
  std::string get_name() const override { return "add_extrabytes"; };

  // multi-threading
  LASRaddattribute* clone() const override { return new LASRaddattribute(*this); };

private:
  int data_type;
  double scale;
  double offset;
  std::string name;
  std::string description;
};

#endif
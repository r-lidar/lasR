#ifndef LASRADDRGB_H
#define LASRADDRGB_H

#include "Stage.h"

class LASRaddrgb: public Stage
{
public:
  bool process(PointCloud*& las) override { return las->add_rgb(); };
  std::string get_name() const override { return "add_rgb"; };
  LASRaddrgb* clone() const override { return new LASRaddrgb(*this); };
};

class LASRremovergb: public Stage
{
public:
  bool process(PointCloud*& las) override { return las->remove_rgb(); };
  std::string get_name() const override { return "remove_rgb"; };
  LASRremovergb* clone() const override { return new LASRremovergb(*this); };
};


#endif
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

#endif
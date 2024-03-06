#ifndef LASRFILTER_H
#define LASRFILTER_H

#include "lasralgorithm.h"

class LASRfilter : public LASRalgorithm
{
public:
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "filter"; };
};

#endif
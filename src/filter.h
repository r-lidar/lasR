#ifndef LASRFILTER_H
#define LASRFILTER_H

#include "Stage.h"

class LASRfilter : public Stage
{
public:
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "filter"; };

  // multi-threading
  LASRfilter* clone() const override { return new LASRfilter(*this); };
};

#endif
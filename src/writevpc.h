#ifndef LASRWRITEVPC_H
#define LASRWRITEVPC_H

#include "lasralgorithm.h"

class LASRvpcwriter: public LASRalgorithm
{
public:
  bool process(LAScatalog*& p) override;
  std::string get_name() const override { return "write_vpc"; }
  bool need_points() const override { return false; }
  bool is_streamable() const override { return true; }
  LASRvpcwriter* clone() const override { return new LASRvpcwriter(*this); };
};

#endif

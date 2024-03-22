#ifndef LASRWRITELAX_H
#define LASRWRITELAX_H

#include "lasralgorithm.h"

class LASRlaxwriter: public LASRalgorithm
{
public:
  bool set_chunk(const Chunk& chunk) override;
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "write_lax"; }
  LASRlaxwriter* clone() const override { return new LASRlaxwriter(*this); };
};

#endif

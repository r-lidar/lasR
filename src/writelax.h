#ifndef LASRWRITELAX_H
#define LASRWRITELAX_H

#include "lasralgorithm.h"
#include "laswriter.hpp"

class LASRlaxwriter: public LASRalgorithm
{
public:
  void set_input_file(std::string file) override;
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "write_lax"; }
};

#endif

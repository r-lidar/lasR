#ifndef NOTHING_H
#define NOTHING_H

#include "lasralgorithm.h"

class LASRnothing : public LASRalgorithm
{
public:
  std::string get_name() const override { return "nothing"; };
  bool is_streamable() const override { return true; };
};
#endif
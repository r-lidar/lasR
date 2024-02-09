#ifndef NOTHING_H
#define NOTHING_H

#include "lasralgorithm.h"

class LASRnothing : public LASRalgorithm
{
public:
  LASRnothing(bool read, bool stream)
  {
    read_points = read | stream;
    streamable  = read_points & stream;
  }
  std::string get_name() const override { return "nothing"; };
  bool is_streamable() const override { return streamable; };
  bool need_points() const override { return read_points; };

  private:
    bool streamable;
    bool read_points;
};
#endif
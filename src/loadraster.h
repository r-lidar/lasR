#ifndef LOADRASTER_H
#define LOADRASTER_H

#include "lasralgorithm.h"

class LASRloadraster : public LASRalgorithmRaster
{
public:
  LASRloadraster(const std::string& file, int band);
  bool set_chunk(const Chunk& chunk) override;
  std::string get_name() const override { return "load_raster"; };
  bool is_streamable() const override { return true; };
  bool need_points() const override { return false; };

  // multi-threading
  LASRloadraster* clone() const override { return new LASRloadraster(*this); };

private:
  int band;
};
#endif
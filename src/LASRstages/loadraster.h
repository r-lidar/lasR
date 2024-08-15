#ifndef LOADRASTER_H
#define LOADRASTER_H

#include "Stage.h"

class LASRloadraster : public StageRaster
{
public:
  LASRloadraster(double xmin, double ymin, double xmax, double ymax);
  bool set_chunk(const Chunk& chunk) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "load_raster"; };
  bool is_streamable() const override { return true; };
  bool need_points() const override { return false; };

  // multi-threading
  LASRloadraster* clone() const override { return new LASRloadraster(*this); };

private:
  int band;
};
#endif
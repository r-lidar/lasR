#ifndef LASRSPIKEFREE_H
#define LASRSPIKEFREE_H

#include "Stage.h"

class LASRspikefree : public StageRaster
{
public:
  LASRspikefree();
  bool process(PointCloud*& las) override;
  double need_buffer() const override { return 2.0; }
  bool set_parameters(const nlohmann::json&) override;
  bool is_parallelized() const override { return true; }
  std::string get_name() const override { return "spikefree"; }

  // multi-threading
  LASRspikefree* clone() const override { return new LASRspikefree(*this); };

private:
  double d_f; // Freeze Distance
  double h_b; // Insertion Buffer Height (vertical delay)
  double res; // Output raster resolution
};

#endif

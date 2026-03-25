#ifndef LASRPTD_H
#define LASRPTD_H

#include "Stage.h"

class LASRptd : public Stage
{
public:
  LASRptd();
  bool process(PointCloud*& las) override;
  double need_buffer() const override { return 30.0; }
  bool set_parameters(const nlohmann::json&) override;
  bool is_parallelized() const override { return true; }
  std::string get_name() const override { return "ptd"; }

  // multi-threading
  LASRptd* clone() const override { return new LASRptd(*this); };

private:
  float seed_resolution_search;
  float max_iteration_angle;
  float max_terrain_angle;
  float max_iteration_distance;
  float spacing;
  float buffer_size;
  float rotation;
  int max_iter;
  int classification;
};

#endif

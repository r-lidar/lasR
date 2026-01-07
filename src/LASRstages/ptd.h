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
  std::string get_name() const override { return "ptd"; }

  // multi-threading
  LASRptd* clone() const override { return new LASRptd(*this); };

private:
  double seed_resolution_search;
  double max_iteration_angle;
  double max_terrain_angle;
  double max_iteration_distance;
  double min_triangle_size;
  double buffer_size;
  int max_iter;
  int classification;
};

#endif

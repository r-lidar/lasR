#ifndef LASRPDT_H
#define LASRPDT_H

#include "Stage.h"
#include "Vector.h"
#include "Shape.h"

class Triangulation;
class Raster;

class LASRpdt : public StageVector
{
public:
  LASRpdt();
  bool process(PointCloud*& las) override;
  double need_buffer() const override { return 30.0; }
  void clear(bool last) override;
  //bool write() override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "pdt"; }

  // multi-threading
  LASRpdt* clone() const override { return new LASRpdt(*this); };

private:
  void interpolate(Raster* r) const;
  //void interpolate(std::vector<double>& x) const;

private:
  double seed_resolution_search;
  double max_iteration_angle;
  double max_terrain_angle;
  double max_iteration_distance;
  double min_triangle_size;
  double offset;
  int classification;

  std::vector<int> index_map;
  Triangulation* d;
  PointCloud* las;
};

#endif

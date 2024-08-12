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
  LASRpdt(double distance, double angle, double res, double min_size, int classification);
  bool process(LAS*& las) override;
  double need_buffer() const override { return 30.0; }
  void clear(bool last) override;
  bool write() override;
  std::string get_name() const override { return "pdt"; }

  // multi-threading
  LASRpdt* clone() const override { return new LASRpdt(*this); };

private:
  void compute_dtm(const Triangulation* d, Raster* r) const;

private:
  double seed_resolution_search;
  double max_iteration_angle;
  double max_terrain_angle;
  double max_iteration_distance;
  double min_triangle_size;
  int classification;

  std::vector<int> index_map;
  Triangulation* d;
  LAS* las;
};

#endif

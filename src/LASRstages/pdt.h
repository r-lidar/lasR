#ifndef LASRPDT_H
#define LASRPDT_H

#include "Stage.h"
#include "Vector.h"
#include "Shape.h"

class Triangulation;

class LASRpdt : public StageVector
{
public:
  LASRpdt();
  bool process(LAS*& las) override;
  double need_buffer() const override { return 50.0; }
  void clear(bool last) override;
  bool write() override;
  std::string get_name() const override { return "Progressive TIN densification"; }

  // multi-threading
  LASRpdt* clone() const override { return new LASRpdt(*this); };

private:
  double seed_resolution_search;
  double max_iteration_angle;
  double max_terrain_angle;
  double max_iteration_distance;
  double min_triangle_size;

  std::vector<int> index_map;
  Triangulation* d;
  LAS* las;
};

#endif

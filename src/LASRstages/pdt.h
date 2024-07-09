#ifndef LASRPDT_H
#define LASRPDT_H

#include "Stage.h"
#include "Vector.h"
#include "Shape.h"

#include "Andrea/Headers/delaunaytriangulationcore.h"

class LASRpdt : public StageVector
{
public:
  LASRpdt(double xmin, double ymin, double xmax, double ymax);
  bool process(LAS*& las) override;
  double need_buffer() const override { return 50.0; }
  void clear(bool last) override;
  bool write() override;
  std::string get_name() const override { return "Progressive TIN densification"; }

  // multi-threading
  LASRpdt* clone() const override { return new LASRpdt(*this); };

private:
  unsigned int npoints;
  std::vector<int> index_map;
  DelaunayTriangulationCore d;
  LAS* las;
};

#endif

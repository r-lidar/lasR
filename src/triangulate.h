#ifndef LASRTRIANGULATE_H
#define LASRTRIANGULATE_H

#include "lasralgorithm.h"
#include "Vector.h"
#include "Shape.h"

#include <unordered_set>

class Raster;

namespace delaunator
{
  class Delaunator;
}

class LASRtriangulate : public LASRalgorithmVector
{
public:
  LASRtriangulate(double xmin, double ymin, double xmax, double ymax, double trim, std::string use_attribute);
  bool process(LAS*& las) override;
  bool interpolate(std::vector<double>& res, const Raster* raster = nullptr);
  bool contour(std::vector<Edge>& edges) const;
  double need_buffer() const override { return 50.0; }
  void clear(bool last) override;
  bool write() override;
  std::string get_name() const override { return "triangulate"; }

private:
  bool keep_large;
  double trim;
  unsigned int npoints;
  std::vector<double> coords;
  std::vector<int> index_map;
  std::string use_attribute;
  delaunator::Delaunator* d;
  LAS* las;
};

#endif

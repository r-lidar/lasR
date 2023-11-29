#ifndef LASRRASTERIZE_H
#define LASRRASTERIZE_H

#include "lasralgorithm.h"
#include "NA.h"

class LASRrasterize : public LASRalgorithmRaster
{
public:
  LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, std::vector<int> methods);
  LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, LASRalgorithm* algorithm);
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return algorithm == 0; };
  bool is_mergable() const override { return algorithm == 0; };
  std::string get_name() const override { return "rasterize"; };

private:
  float pmax  (float x, float y) { if (x == NA_F32_RASTER) return y; return (x > y) ? x : y; }
  float pmin  (float x, float y) { if (x == NA_F32_RASTER) return y; return (x < y) ? x : y; }
  float pcount(float x, float y) { if (x == NA_F32_RASTER) return 1; return x+1; }
  typedef float (LASRrasterize::*FunctionPointer)(float x, float y);
  std::vector<FunctionPointer> operators;
  LASRalgorithm* algorithm; // Not the owner
};

#endif

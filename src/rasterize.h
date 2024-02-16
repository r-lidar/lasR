#ifndef LASRRASTERIZE_H
#define LASRRASTERIZE_H

#include "lasralgorithm.h"
#include "NA.h"

class LASRrasterize : public LASRalgorithmRaster
{
public:
  LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, double window, std::vector<int> methods);
  LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, LASRalgorithm* algorithm);
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  double need_buffer() const override { return MAX(raster.get_xres(), window); };
  bool is_streamable() const override { return connections.size() == 0; };
  std::string get_name() const override { return "rasterize"; };

private:
  double window;
  float pmax  (float x, float y) { if (x == NA_F32_RASTER) return y; return (x > y) ? x : y; }
  float pmin  (float x, float y) { if (x == NA_F32_RASTER) return y; return (x < y) ? x : y; }
  float pcount(float x, float y) { if (x == NA_F32_RASTER) return 1; return x+1; }
  typedef float (LASRrasterize::*FunctionPointer)(float x, float y);
  std::vector<FunctionPointer> operators;
};

#endif

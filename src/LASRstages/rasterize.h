#ifndef LASRRASTERIZE_H
#define LASRRASTERIZE_H

#include "Stage.h"
#include "Metrics.h"
#include "NA.h"

class LASRrasterize : public StageRaster
{
public:
  LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, double window, const std::vector<std::string>& methods, float default_value);
  LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, Stage* algorithm);
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  double need_buffer() const override { return MAX(raster.get_xres(), window); };
  bool is_streamable() const override { return streamable; };
  bool is_parallelized() const override { return !streamable; };
  std::string get_name() const override { return "rasterize"; };

  // multi-threading
  LASRrasterize* clone() const override { return new LASRrasterize(*this); };

private:
  bool streamable;
  double window;
  Metrics metrics;
};

#endif

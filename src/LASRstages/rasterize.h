#ifndef LASRRASTERIZE_H
#define LASRRASTERIZE_H

#include "Stage.h"
#include "Metrics.h"
#include "NA.h"

class LASRrasterize : public StageRaster
{
public:
  LASRrasterize() = default;
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  double need_buffer() const override { return MAX(raster.get_xres(), window); };
  bool is_streamable() const override { return streamable; };
  bool is_parallelized() const override { return !streamable; };
  bool set_parameters(const nlohmann::json&) override;
  bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uuid) override;
  std::string get_name() const override { return "rasterize"; };

  // multi-threading
  LASRrasterize* clone() const override { return new LASRrasterize(*this); };

private:
  bool streamable;
  double window;
  MetricManager metric_engine;
};

#endif

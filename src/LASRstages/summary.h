#ifndef LASRSUMMARY_H
#define LASRSUMMARY_H

#include "Stage.h"
#include "Metrics.h"

#include <map>
#include <vector>
#include <inttypes.h>

class LASRsummary: public Stage
{
public:
  LASRsummary();
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return !metrics_engine.active(); }
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "summary"; }

  // multi-threading
  bool is_parallelizable() const override { return true; };
  LASRsummary* clone() const override { return new LASRsummary(*this); };
  void merge(const Stage* other) override;
  void sort(const std::vector<int>& order) override;

  #ifdef USING_R
  SEXP to_R() override;
  #endif

  nlohmann::json to_json() const override;

private:
  void merge_maps(std::map<int, uint64_t>& map1, const std::map<int, uint64_t>& map2);

private:
  uint64_t npoints;
  uint64_t nsingle;
  uint64_t nwithheld;
  uint64_t nsynthetic;
  double zwbin;
  double iwbin;
  std::map<int, uint64_t> npoints_per_return;
  std::map<int, uint64_t> npoints_per_class;
  std::map<int, uint64_t> npoints_per_sdf;
  std::map<int, uint64_t> zhistogram;
  std::map<int, uint64_t> ihistogram;
  std::map<std::string, std::vector<float>> metrics;

  MetricManager metrics_engine;
  PointCollection cloud;
};

#endif

#ifndef LASRSUMMARY_H
#define LASRSUMMARY_H

#include "Stage.h"

#include <map>
#include <vector>
#include <inttypes.h>

class LASRsummary: public Stage
{
public:
  LASRsummary(double xmin, double ymin, double xmax, double ymax, double zwbin, double iwbin);
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return true; }
  std::string get_name() const override { return "summary"; }

  // multi-threading
  bool is_parallelizable() const override { return true; };
  LASRsummary* clone() const override { return new LASRsummary(*this); };
  void merge(const Stage* other) override;

  #ifdef USING_R
  SEXP to_R() override;
  #endif

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
};

#endif

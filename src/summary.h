#ifndef LASRSUMMARY_H
#define LASRSUMMARY_H

#include "lasralgorithm.h"

#include <map>
#include <vector>
#include <inttypes.h>

class LASRsummary: public LASRalgorithm
{
public:
  LASRsummary(double xmin, double ymin, double xmax, double ymax, double zwbin, double iwbin);
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return true; }
  bool is_mergable() const override { return true; }
  std::string get_name() const override { return "summary"; }

#ifdef USING_R
  SEXP to_R() override;
#endif

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

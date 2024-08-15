#ifndef LASRAGGREGATE_H
#define LASRAGGREGATE_H

#ifdef USING_R

#include "Stage.h"
#include "GridPartition.h" // Must create a file Grouper

class LASRaggregate: public StageRaster
{
public:
  LASRaggregate(double xmin, double ymin, double xmax, double ymax);
  bool process(LAS*& las) override;
  void clear(bool last) override;
  double need_buffer() const override { return MAX(raster.get_xres(), window); };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "aggregate"; };
  bool use_rcapi() const override { return true; };

  // multi-threading
  LASRaggregate* clone() const override { return new LASRaggregate(*this); };

private:
  // for consistency check
  int nmetrics;
  int expected_type;

  // for moving windows rasterization
  double window;

  // R expression and call environment
  SEXP call;
  SEXP env;

  Grouper grouper;

  enum attributes{X, Y, Z, I, T, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, UD, SA, PSID, R, G, B, NIR, CHAN};
};

#else
#pragma message("LASaggregate skipped: cannot be compiled without R")
#endif

#endif
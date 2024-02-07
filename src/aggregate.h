#ifndef LASRAGGREGATE_H
#define LASRAGGREGATE_H

#ifdef USING_R

#include "lasralgorithm.h"
#include "GridPartition.h" // Must create a file Grouper

class LASRaggregate: public LASRalgorithmRaster
{
public:
  LASRaggregate(double xmin, double ymin, double xmax, double ymax, double res, double window, SEXP call, SEXP env);
  bool process(LAS*& las) override;
  void clear(bool last) override;
  double need_buffer() const override { return raster.get_xres(); }
  std::string get_name() const override { return "aggregate"; };

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
#ifndef IVF_H
#define IVF_H

#include "lasralgorithm.h"

class LASRnoiseivf: public LASRalgorithm
{
public:
  LASRnoiseivf(double xmin, double ymin, double xmax, double ymax, double res, int n, int classification);
  bool process(LAS*& las) override;
  double need_buffer() const override { return res; };
  std::string get_name() const override { return "ivf"; };

  // multi-threading
  LASRnoiseivf* clone() const override { return new LASRnoiseivf(*this); };

private:
  double res;
  int n;
  int classification;

};

#endif
#ifndef IVF_H
#define IVF_H

#include "Stage.h"

class LASRivf: public Stage
{
public:
  LASRivf(double xmin, double ymin, double xmax, double ymax, double res, int n, int classification, bool force_map);
  bool process(LAS*& las) override;
  double need_buffer() const override { return res; };
  std::string get_name() const override { return "ivf"; };

  // multi-threading
  LASRivf* clone() const override { return new LASRivf(*this); };

private:
  double res;
  int n;
  int classification;
  bool force_map;

};

#endif
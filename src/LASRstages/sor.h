#ifndef SOR_H
#define SOR_H

#include "Stage.h"

class LASRsor: public Stage
{
public:
  LASRsor(double xmin, double ymin, double xmax, double ymax, int k, int m, int classification);
  bool process(LAS*& las) override;
  double need_buffer() const override { return 10; };
  std::string get_name() const override { return "sor"; };

  // multi-threading
  LASRsor* clone() const override { return new LASRsor(*this); };

private:
  int k;
  int m;
  int classification;
};

#endif
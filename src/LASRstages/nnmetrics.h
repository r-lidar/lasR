#ifndef NNMETRICS_H
#define NNMETRICS_H

#include "Stage.h"

class LASRnnmetrics : public StageVector
{
public:
  LASRnnmetrics(int k, double r, const std::vector<std::string>& methods, Stage* algorithm);
  bool process(LAS*& las) override;
  std::string get_name() const override { return "neighborhood_metrics"; }
  bool is_parallelized() const override { return true; }
  LASRnnmetrics* clone() const override { return new LASRnnmetrics(*this); };

private:
  int mode;
  int k;
  double r;
};

#endif

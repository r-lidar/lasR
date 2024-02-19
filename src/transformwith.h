#ifndef LASRTRANSFORMWITH_H
#define LASRTRANSFORMWITH_H

#include "lasralgorithm.h"

class LASRtriangulate;

class LASRtransformwith: public LASRalgorithm
{
public:
  LASRtransformwith(double xmin, double ymin, double xmax, double ymax, LASRalgorithm* algorithm, std::string op, std::string attribute);
  bool process(LAS*& las) override;
  std::string get_name() const override { return "transform_with"; }
  LASRtransformwith* clone() const override { return new LASRtransformwith(*this); };

private:
  int op;
  std::string attribute;
  enum operators {ADD, SUB};
};

#endif
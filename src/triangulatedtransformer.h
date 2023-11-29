#ifndef LASRTRIANGULATEDTRANFORMER_H
#define LASRTRIANGULATEDTRANFORMER_H

#include "lasralgorithm.h"

class LASRtriangulatedTransformer: public LASRalgorithm
{
public:
  LASRtriangulatedTransformer(double xmin, double ymin, double xmax, double ymax, LASRalgorithm* algorithm, std::string op, std::string attribute);
  bool process(LAS*& las) override;
  std::string get_name() const override { return "transform_with_triangulation"; }

private:
  LASRalgorithm* algorithm; // Not the owner
  int op;
  std::string attribute;
  enum operators {ADD, SUB};
};

#endif
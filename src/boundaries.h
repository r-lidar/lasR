#ifndef BOUNDARIES_H
#define BOUNDARIES_H

#include "lasralgorithm.h"
#include "Vector.h"

class LASRboundaries : public LASRalgorithmVector
{
public:
  LASRboundaries(double xmin, double ymin, double xmax, double ymax, LASRalgorithm* algorithm);
  bool process(LASheader*& header) override;
  bool process(LAS*& las) override;
  void clear(bool last) override;
  bool write() override;
  bool need_points() const override;
  bool is_streamable() const override { return true; }
  std::string get_name() const override { return "hulls"; }
  bool is_parallelized() const override { return true; };

  // multi-threading
  LASRboundaries* clone() const override { return new LASRboundaries(*this); }

private:
  std::vector<PolygonXY> contour;
};

#endif

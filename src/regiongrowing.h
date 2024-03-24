#ifndef REGIONGROWING_H
#define REGIONGROWING_H

#include "Stage.h"

class LASRregiongrowing : public StageRaster
{
public:
  LASRregiongrowing(double xmin, double ymin, double xmax, double ymax,  double th_seed, double th_crown, double th_tree, double DIST, Stage* algorithm_input_rasters, Stage* algorithm_input_seeds);
  bool process(LAS*& las) override;
  double need_buffer() const override { return 50; };
  std::string get_name() const override { return "region_growing"; }

  // multi-threading
  LASRregiongrowing* clone() const override { return new LASRregiongrowing(*this); };


private:
  double th_seed;
  double th_crown;
  double th_tree;
  double DIST;
};

#endif
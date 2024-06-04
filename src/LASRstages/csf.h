#ifndef IVF_H
#define IVF_H

#include "Stage.h"

class LASRcsf: public Stage
{
public:
  LASRcsf(double xmin, double ymin, double xmax, double ymax, bool slope_smooth, float class_threshold, float cloth_resolution, int rigidness, int iterations, float time_step);
  bool process(LAS*& las) override;
  double need_buffer() const override { return true; };
  std::string get_name() const override { return "csf"; };

  // multi-threading
  LASRcsf* clone() const override { return new LASRnoiseivf(*this); };

private:
  bool slope_smooth;
  float class_threshold;
  float cloth_resolution;
  int  rigidness;
  int iterations;
  float time_step;
};

#endif
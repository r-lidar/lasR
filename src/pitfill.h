#ifndef PITFILL_H
#define PITFILL_H

#include "lasralgorithm.h"

class LASRpitfill: public LASRalgorithmRaster
{
public:
  LASRpitfill(double xmin, double ymin, double xmax, double ymax, int lap_size, float thr_lap, float thr_spk, int med_size, float dil_radius, LASRalgorithm* algorithm);
  bool process(LAS*& las) override;
  bool is_streamable() const override { return algorithm == 0; };
  double need_buffer() const override;
  std::string get_name() const override { return "pit_fill"; }

private:
  LASRalgorithm* algorithm; // Not the owner
  int lap_size;
  float thr_lap;
  float thr_spk;
  int med_size;
  int dil_radius;
};

#endif
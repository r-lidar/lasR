#ifndef PITFILL_H
#define PITFILL_H

#include "Stage.h"

class LASRpitfill: public StageRaster
{
public:
  LASRpitfill(double xmin, double ymin, double xmax, double ymax, int lap_size, float thr_lap, float thr_spk, int med_size, float dil_radius, Stage* algorithm);
  bool process() override;
  double need_buffer() const override;
  std::string get_name() const override { return "pit_fill"; }

  // multi-threading
  LASRpitfill* clone() const override { return new LASRpitfill(*this); };

private:
  int lap_size;
  float thr_lap;
  float thr_spk;
  int med_size;
  int dil_radius;
};

#endif
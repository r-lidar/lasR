#ifndef CSF_H
#define CSF_H

#include "Stage.h"

class LASRcsf: public Stage
{
public:
  LASRcsf();
  bool process(LAS*& las) override;
  double need_buffer() const override { return cloth_resolution*5; };
  bool is_parallelized() const override { return true; };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "csf"; };

  // multi-threading
  LASRcsf* clone() const override { return new LASRcsf(*this); };

private:
  bool slope_smooth;
  float class_threshold;
  float cloth_resolution;
  int  rigidness;
  int iterations;
  float time_step;
  int classification;
};

#endif
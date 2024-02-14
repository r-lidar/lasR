#ifndef SAMPLINGVOXEL_H
#define SAMPLINGVOXEL_H

#include "lasralgorithm.h"

class LASRsamplingvoxels : public LASRalgorithm
{
public:
  LASRsamplingvoxels(double xmin, double ymin, double xmax, double ymax, double res);
  bool process(LAS*& las) override;
  double need_buffer() const override { return res; }
  std::string get_name() const override { return "voxel_sampling"; }
  LASRsamplingvoxels* clone() const override { return new LASRsamplingvoxels(*this); };

private:
  double res;
};

class LASRsamplingpixels : public LASRalgorithm
{
public:
  LASRsamplingpixels(double xmin, double ymin, double xmax, double ymax, double res);
  bool process(LAS*& las) override;
  double need_buffer() const override { return res; }
  std::string get_name() const override { return "pixel_sampling"; }
  LASRsamplingpixels* clone() const override { return new LASRsamplingpixels(*this); };

private:
  double res;
};

#endif
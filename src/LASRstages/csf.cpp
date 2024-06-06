#include "csf.h"
#include "csf/CSF.h"

LASRcsf::LASRcsf(double xmin, double ymin, double xmax, double ymax, bool slope_smooth, float class_threshold, float cloth_resolution, int rigidness, int iterations, float time_step, int classificiation)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  this->slope_smooth = slope_smooth;
  this->class_threshold = class_threshold;
  this->cloth_resolution = cloth_resolution;
  this->rigidness = rigidness;
  this->iterations = iterations;
  this->time_step = time_step;
  this->classification = classificiation;
}

bool LASRcsf::process(LAS*& las)
{
  progress->reset();
  progress->set_prefix("CSF");
  progress->show();

  std::vector<csf::Point> points;
  points.reserve(las->npoints);
  while(las->read_point())
  {
    csf::Point p;
    p.x = las->point.get_x();
    p.y = las->point.get_y();
    p.z = las->point.get_z();
    if (las->point.get_classification() == classification)
    {
      las->point.set_classification(0);
      las->update_point();
    }
    points.push_back(p);
  }

  CSF csf;
  csf.params.bSloopSmooth = slope_smooth;
  csf.params.class_threshold = class_threshold;
  csf.params.cloth_resolution = cloth_resolution;
  csf.params.interations = iterations;
  csf.params.rigidness = rigidness;
  csf.params.time_step = time_step;
  csf.ncpu = ncpu;
  csf.setPointCloud(points);

  std::vector<int> ground, nonground;
  csf.do_filtering(ground, nonground);

  int i = 0;
  int k = 0;
  while(las->read_point())
  {
    if (i == ground[k])
    {
      k++;
      if (las->point.classification != 9) // Preserve water
      {
        las->point.set_classification(classification);
        las->update_point();
      }
    }
    i++;
  }

  return true;
}

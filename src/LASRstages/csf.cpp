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
  csf.do_filtering(ground, nonground, false);

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




/*
// Experimental variation

 //#include "Raster.h"
 //#include "NA.h"
 //#include <algorithm>

bool use_low = false;

if (use_low)
{
  // Create a raster to filter lowest point with a grid
  print("%lf, %lf, %lf, %lf", las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y);
  Raster min(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, 0.5);
  min.set_value(0, NA_F32_RASTER); // Initialize the memory layout

  // The maximum number of point to analyse is the number of celld in the grid
  points.reserve(min.get_ncells());

  // Record the IDs of the lowest points
  low_points.resize(min.get_ncells());
  std::fill(low_points.begin(), low_points.end(), -1);

  while(las->read_point(true))
  {
    int cell = min.cell_from_xy(las->point.get_x(), las->point.get_y());
    if (cell < 0 || cell > min.get_ncells())
    {
      last_error = "Internal error: inccorect cell";
      return false;
    }

    float val = min.get_value(cell);

    if (val == NA_F32_RASTER)
    {
      min.set_value(cell, las->point.get_z());
      low_points[cell] = las->current_point;
    }
    else if (las->point.get_z() < val)
    {
      min.set_value(cell, las->point.get_z());
      low_points[cell] = las->current_point;
    }
  }

  auto end = std::remove(low_points.begin(), low_points.end(), -1);

  for (auto it = low_points.begin() ; it < end ; it++)
  {
    las->seek(*it);
    //print("Add point %d\n", i);

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
}
else
{
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

print("ground size = %lu\n", ground.size());

if (use_low)
{
  for (auto i : ground)
  {
    int j = low_points[i];
    if (j == -1) continue;

    las->seek(j);
    if (las->point.classification != 9) // Preserve water
    {
      las->point.set_classification(classification);
      las->update_point();
    }
  }
}
else
{
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
}
*/
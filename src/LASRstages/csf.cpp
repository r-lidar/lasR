#include "csf.h"
#include "csf/CSF.h"

LASRcsf::LASRcsf()
{
}

bool LASRcsf::process(PointCloud*& las)
{
  progress->reset();
  progress->set_prefix("CSF");
  progress->show();

  std::vector<csf::Point> points;
  points.reserve(las->npoints);

  AttributeAccessor set_and_get_classification("Classification");

  while(las->read_point())
  {
    if (pointfilter.filter(&las->point)) continue;

    csf::Point p;
    p.x = las->point.get_x();
    p.y = las->point.get_y();
    p.z = las->point.get_z();
    if (set_and_get_classification(&las->point) == (double)classification) set_and_get_classification(&las->point, 0);
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
    if (pointfilter.filter(&las->point)) continue;

    if (i == ground[k])
    {
      k++;
      if (set_and_get_classification(&las->point) != 9) set_and_get_classification(&las->point, classification);
    }
    i++;
  }

  return true;
}

bool LASRcsf::set_parameters(const nlohmann::json& stage)
{
  slope_smooth = stage.value("slope_smooth", false);
  class_threshold = stage.value("class_threshold", 0.5);
  cloth_resolution = stage.value("cloth_resolution", 0.5);
  rigidness = stage.value("rigidness", 1);
  iterations = stage.value("iterations", 500);
  time_step = stage.value("time_step", 0.65);
  classification = stage.value("class", 2);

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
    int cell = min.cell_from_xy(las->pointoint.get_x(), las->pointoint.get_y());
    if (cell < 0 || cell > min.get_ncells())
    {
      last_error = "Internal error: inccorect cell";
      return false;
    }

    float val = min.get_value(cell);

    if (val == NA_F32_RASTER)
    {
      min.set_value(cell, las->pointoint.get_z());
      low_points[cell] = las->current_point;
    }
    else if (las->pointoint.get_z() < val)
    {
      min.set_value(cell, las->pointoint.get_z());
      low_points[cell] = las->current_point;
    }
  }

  auto end = std::remove(low_points.begin(), low_points.end(), -1);

  for (auto it = low_points.begin() ; it < end ; it++)
  {
    las->seek(*it);
    //print("Add point %d\n", i);

    csf::Point p;
    p.x = las->pointoint.get_x();
    p.y = las->pointoint.get_y();
    p.z = las->pointoint.get_z();
    if (las->pointoint.get_classification() == classification)
    {
      las->pointoint.set_classification(0);
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
    p.x = las->pointoint.get_x();
    p.y = las->pointoint.get_y();
    p.z = las->pointoint.get_z();
    if (las->pointoint.get_classification() == classification)
    {
      las->pointoint.set_classification(0);
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
    if (las->pointoint.classification != 9) // Preserve water
    {
      las->pointoint.set_classification(classification);
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
      if (las->pointoint.classification != 9) // Preserve water
      {
        las->pointoint.set_classification(classification);
        las->update_point();
      }
    }
    i++;
  }
}
*/
#include "noiseivf.h"
#include <unordered_map>

LASRnoiseivf::LASRnoiseivf(double xmin, double ymin, double xmax, double ymax, double res, int n, int classification)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->res = res;
  this->n = n;
  this->classification = classification;
}

bool LASRnoiseivf::process(LAS*& las)
{
  double xmin = las->header->min_x;
  double ymin = las->header->min_y;
  double zmin = las->header->min_z;
  double xmax = las->header->max_x;
  double ymax = las->header->max_y;
  //double zmax = las->header->max_z;

  int width = std::floor((xmax - xmin) / res);
  int height = std::floor((ymax - ymin) / res);
  //int depth = std::floor((zmax - zmin) / res);

  // Stores for a given voxel the number of point in its 27 voxels neighbourhood
  std::unordered_map<int, int> dynamic_registry;

  while(las->read_point())
  {
    int nx = std::floor((las->point.get_x() - xmin) / res);
    int ny = std::floor((las->point.get_y() - ymin) / res);
    int nz = std::floor((las->point.get_z() - zmin) / res);

    // Add one in the 27 neighbouring voxel of this point
    for (int i : {-1,0,1})
    {
      for (int j : {-1,0,1})
      {
        for (int k : {-1,0,1})
        {
          if (!(i == 0 && j == 0 && k == 0))
          {
            int key = (nx+i) + (ny+j)*width + (nz+k)*width*height;
            dynamic_registry.insert({key, 0});
            dynamic_registry[key]++;
          }
        }
      }
    }
  }

  // Loop again through each point.
  // Check if the number of points in its neighbourhood is above the threshold
  while(las->read_point())
  {
    int nx = std::floor((las->point.get_x() - xmin) / res);
    int ny = std::floor((las->point.get_y() - ymin) / res);
    int nz = std::floor((las->point.get_z() - zmin) / res);
    int key = nx + ny*width + nz*width*height;
    if (dynamic_registry[key] <= n)
    {
      las->point.set_classification(classification);
      las->update_point();
    }
  }

  return true;
}

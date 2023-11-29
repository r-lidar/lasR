#include "sampling.h"
#include "Progress.hpp"
#include "Grid.h"

#include <unordered_set>
#include <algorithm>
#include <random>

LASRsamplingvoxels::LASRsamplingvoxels(double xmin, double ymin, double xmax, double ymax, double res)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->res = res;
}

bool LASRsamplingvoxels::process(LAS*& las)
{
  std::unordered_set<int> registry;

  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  std::shuffle(index.begin(), index.end(), std::mt19937{std::random_device{}()});

  double rxmin = las->header->min_x;
  double rymin = las->header->min_y;
  double rzmin = las->header->min_z;
  double rxmax = las->header->max_x;
  double rymax = las->header->max_y;

  rxmin = ROUNDANY(rxmin - 0.5*res, res);
  rymin = ROUNDANY(rxmin - 0.5*res, res);
  rzmin = ROUNDANY(rzmin - 0.5*res, res);
  rxmax = ROUNDANY(rxmax + 0.5*res, res);
  rymax = ROUNDANY(rxmax + 0.5*res, res);
  //double rzmax = las->header->max_z;

  int width = (rxmax - rxmin)/res;
  int height = (rymax - rymin)/res;

  progress->reset();
  progress->set_prefix("voxel sampling");
  progress->set_total(index.size());

  for (int i : index)
  {
    las->seek(i);
    if (las->point.get_withheld_flag() != 0) continue;

    int nx = std::floor((las->point.get_x() - rxmin) / res);
    int ny = std::floor((las->point.get_y() - rymin) / res);
    int nz = std::floor((las->point.get_z() - rzmin) / res);
    int key = nx + ny*width + nz*width*height;

    if (registry.find(key) == registry.end())
      registry.insert(key);
    else
      las->remove_point();

    (*progress)++;
    progress->show();
  }

  return true;
}

LASRsamplingpixels::LASRsamplingpixels(double xmin, double ymin, double xmax, double ymax, double res)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->res = res;
}

bool LASRsamplingpixels::process(LAS*& las)
{
  std::unordered_set<int> registry;

  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  std::shuffle(index.begin(), index.end(), std::mt19937{std::random_device{}()});

  double rxmin = las->header->min_x;
  double rymin = las->header->min_y;
  double rzmin = las->header->min_z;
  double rxmax = las->header->max_x;
  double rymax = las->header->max_y;
  Grid grid(rxmin, rymin, rxmax, rymax, res);

  progress->reset();
  progress->set_prefix("voxel sampling");
  progress->set_total(index.size());

  for (int i : index)
  {
    las->seek(i);
    if (las->point.get_withheld_flag() != 0) continue;

    int key = grid.cell_from_xy(las->point.get_x(), las->point.get_y());

    if (registry.find(key) == registry.end())
      registry.insert(key);
    else
      las->remove_point();

    (*progress)++;
    progress->show();
  }

  return true;
}
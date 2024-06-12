#include "sampling.h"
#include "Grid.h"

#include <unordered_set>
#include <algorithm>
#include <random>

// POISSON

LASRsamplingpoisson::LASRsamplingpoisson(double xmin, double ymin, double xmax, double ymax, double distance)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->distance = distance;
}

bool LASRsamplingpoisson::process(LAS*& las)
{
  double r_square = distance*distance;
  double res = distance; // Cell size for the grid

  std::unordered_map<int, std::vector<PointXYZ>> grid;

  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  std::shuffle(index.begin(), index.end(), std::mt19937{std::random_device{}()});

  double rxmin = las->header->min_x;
  double rymin = las->header->min_y;
  double rzmin = las->header->min_z;
  double rxmax = las->header->max_x;
  double rymax = las->header->max_y;

  rxmin = ROUNDANY(rxmin - 0.5 * res, res);
  rymin = ROUNDANY(rymin - 0.5 * res, res);
  rzmin = ROUNDANY(rzmin - 0.5 * res, res);
  rxmax = ROUNDANY(rxmax + 0.5 * res, res);
  rymax = ROUNDANY(rymax + 0.5 * res, res);

  int width = (rxmax - rxmin) / res;
  int height = (rymax - rymin) / res;

  progress->reset();
  progress->set_prefix("Poisson disk sampling");
  progress->set_total(index.size());

  int n = 0;

  for (int i : index)
  {
    las->seek(i);
    if (las->point.get_withheld_flag() != 0) continue;

    double px = las->point.get_x();
    double py = las->point.get_y();
    double pz = las->point.get_z();

    // Voxel of this point
    int nx = std::floor((px - rxmin) / res);
    int ny = std::floor((py - rymin) / res);
    int nz = std::floor((pz - rzmin) / res);
    int key = nx + ny*width + nz*width*height;

    // Do we retain this point? We look into the 27 neighbors to figure out if it is not to close to already inserted points
    bool valid = true;
    for (int dx = -1; dx <= 1 && valid; ++dx)
    {
      for (int dy = -1; dy <= 1 && valid; ++dy)
      {
        for (int dz = -1; dz <= 1; ++dz)
        {
          int key2 = (nx+dx) + (ny+dy)*width + (nz+dz)*width*height;

          // If there are points the voxel
          if (grid.find(key2) != grid.end())
          {
            for (const auto& neighbor : grid[key2])
            {
              double dist_square = (px - neighbor.x) * (px - neighbor.x) +  (py - neighbor.y) * (py - neighbor.y) + (pz - neighbor.z) * (pz - neighbor.z);
              if (dist_square < r_square)
              {
                valid = false;
                break;
              }
            }
          }
        }
      }
    }

    if (valid)
    {
      grid[key].emplace_back(px, py, pz);
      n++;
    }
    else
    {
      las->remove_point();
    }

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  las->update_header();

  // In lasR, deleted points are not actually deleted. They are withhelded, skipped by each stage but kept
  // to avoid the cost of memory reallocation and memmove. Here, if we remove more than 33% of the points
  // actually remove the points. This will save some computation later.
  double ratio = (double)n/(double)las->npoints;
  if (ratio > 1/3)
  {
    if (!las->delete_withheld()) return false;
  }

  return true;
}

// VOXEL

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

  rxmin = ROUNDANY(rxmin - 0.5 * res, res);
  rymin = ROUNDANY(rymin - 0.5 * res, res);
  rzmin = ROUNDANY(rzmin - 0.5 * res, res);
  rxmax = ROUNDANY(rxmax + 0.5 * res, res);
  rymax = ROUNDANY(rymax + 0.5 * res, res);
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

    // Voxel of this point
    int nx = std::floor((las->point.get_x() - rxmin) / res);
    int ny = std::floor((las->point.get_y() - rymin) / res);
    int nz = std::floor((las->point.get_z() - rzmin) / res);
    int key = nx + ny*width + nz*width*height;

    // Do we retain this point ? We look into the registry to know if the voxel exist. If not, we retain the point.
    if (registry.find(key) == registry.end())
      registry.insert(key);
    else
      las->remove_point();

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  las->update_header();

  return true;
}

// PIXEL

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

    // Pixel of this point
    int key = grid.cell_from_xy(las->point.get_x(), las->point.get_y());

    // Do we retain this point ? We look into the registry to know if the pixel exist. If not, we retain the point.
    if (registry.find(key) == registry.end())
      registry.insert(key);
    else
      las->remove_point();

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  las->update_header();

  return true;
}
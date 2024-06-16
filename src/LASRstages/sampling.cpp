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

  std::mt19937 rng(0);
  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  std::shuffle(index.begin(), index.end(), rng);

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

  int length = (rxmax - rxmin) / res;
  int width = (rymax - rymin) / res;

  progress->reset();
  progress->set_prefix("Poisson disk sampling");
  progress->set_total(index.size());

  int n = 0;

  int k = 0;
  for (int i : index)
  {
    las->seek(i);
    if (las->point.get_withheld_flag() != 0) continue;
    if (lasfilter.filter(&las->point))
    {
      las->remove_point();
      continue;
    }

    double px = las->point.get_x();
    double py = las->point.get_y();
    double pz = las->point.get_z();

    // Voxel of this point
    int nx = std::floor((px - rxmin) / res);
    int ny = std::floor((py - rymin) / res);
    int nz = std::floor((pz - rzmin) / res);
    int key = nx + ny*length + nz*length*width;

    // Do we retain this point? We look into the 27 neighbors to figure out if it is not to close to already inserted points
    bool valid = true;

    // Check the central voxel, this should be enough in most cases and allows to skip the 26 neighbors
    int key2 = (nx+0) + (ny+0)*length + (nz+0)*length*width;
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
        k++;
      }
    }

    for (int dx = -1; dx <= 1 && valid; ++dx)
    {
      for (int dy = -1; dy <= 1 && valid; ++dy)
      {
        for (int dz = -1; dz <= 1; ++dz)
        {
          if (dx == 0 && dy == 0 && dz == 0)
            continue;

          key2 = (nx+dx) + (ny+dy)*length + (nz+dz)*length*width;

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

  progress->done();

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
  std::vector<bool> bitregistry;
  bool use_bitregistry = false;

  std::mt19937 rng(0);
  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  std::shuffle(index.begin(), index.end(), rng);

  double rxmin = las->header->min_x;
  double rymin = las->header->min_y;
  double rzmin = las->header->min_z;
  double rxmax = las->header->max_x;
  double rymax = las->header->max_y;
  double rzmax = las->header->max_z;

  rxmin = ROUNDANY(rxmin - 0.5 * res, res);
  rymin = ROUNDANY(rymin - 0.5 * res, res);
  rzmin = ROUNDANY(rzmin - 0.5 * res, res);
  rxmax = ROUNDANY(rxmax + 0.5 * res, res);
  rymax = ROUNDANY(rymax + 0.5 * res, res);
  rzmax = ROUNDANY(rzmax + 0.5 * res, res);

  int length = (rxmax - rxmin)/res;
  int width = (rymax - rymin)/res;
  int height = (rzmax - rzmin)/res;

  size_t nvoxels = (size_t)length*(size_t)width*(size_t)height;
  if (nvoxels < 1024e9) // 128 MB
  {
    print("Use bit registry\n");
    bitregistry.resize(nvoxels);
    use_bitregistry = true;
  }

  progress->reset();
  progress->set_prefix("voxel sampling");
  progress->set_total(index.size());

  int n = 0;

  for (int i : index)
  {
    las->seek(i);
    if (las->point.get_withheld_flag() != 0) continue;
    if (lasfilter.filter(&las->point))
    {
      las->remove_point();
      continue;
    }

    // Voxel of this point
    int nx = std::floor((las->point.get_x() - rxmin) / res);
    int ny = std::floor((las->point.get_y() - rymin) / res);
    int nz = std::floor((las->point.get_z() - rzmin) / res);
    int key = nx + ny*length + nz*length*width;

    // Do we retain this point ? We look into the registry to know if the voxel exist. If not, we retain the point.
    if (use_bitregistry)
    {
      if (!bitregistry[key])
      {
        bitregistry[key] = true;
        n++;
      }
      else
      {
        las->remove_point();
      }
    }
    else
    {
      if (registry.insert(key).second)
        n++;
      else
        las->remove_point();
    }

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  progress->done();

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
  std::vector<bool> bitregistry;
  bool use_bitregistry = false;

  std::mt19937 rng(0);
  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  std::shuffle(index.begin(), index.end(), rng);

  double rxmin = las->header->min_x;
  double rymin = las->header->min_y;
  double rxmax = las->header->max_x;
  double rymax = las->header->max_y;
  Grid grid(rxmin, rymin, rxmax, rymax, res);

  size_t npixels = grid.get_ncells();
  if (npixels < 1024e9) // 128 MB
  {
    bitregistry.resize(npixels);
    use_bitregistry = true;
  }

  progress->reset();
  progress->set_prefix("pixel sampling");
  progress->set_total(index.size());

  int n = 0;

  for (int i : index)
  {
    las->seek(i);
    if (las->point.get_withheld_flag() != 0) continue;
    if (lasfilter.filter(&las->point))
    {
      las->remove_point();
      continue;
    }

    // Pixel of this point
    int key = grid.cell_from_xy(las->point.get_x(), las->point.get_y());

    // Do we retain this point ? We look into the registry to know if the pixel exist. If not, we retain the point.
    if (use_bitregistry)
    {
      if (!bitregistry[key])
      {
        bitregistry[key] = true;
        n++;
      }
      else
      {
        las->remove_point();
      }
    }
    else
    {
      if (registry.insert(key).second)
        n++;
      else
        las->remove_point();
    }

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  progress->done();

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
#include "sampling.h"
#include "Grid.h"

#include <unordered_map>
#include <unordered_set>

/*
 * Comment on computing strategies. There are two computing strategies:
 *
 * 1. using unordered_map or unordered_set do DO NOT alloc memory for each pixel/voxel (especially
 *    voxels) because most of them are not populated. This is memory efficient for large datasets and
 *    tiny resolutions where the number of voxels would be huge.
 * 2. using a bitset of a std::vector. ALL pixels/voxels are allocated. This is much faster (because of the
 *    cost of the hash map mainly) but may require to alloc a lot of memory.
 */

// POISSON SAMPLING
bool LASRsamplingpoisson::set_parameters(const nlohmann::json& stage)
{
  distance = stage.at("distance");
  shuffle_size = stage.value("shuffle_size", 10000);
  return true;
}

bool LASRsamplingpoisson::process(LAS*& las)
{
  double r_square = distance*distance;
  double res = distance; // Cell size for the grid

  std::unordered_map<Voxel, std::vector<PointXYZ>, VoxelHash> uregistry;
  std::vector<std::vector<PointXYZ>> vregistry;
  bool use_vregistry = false;

  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  shuffle(index, shuffle_size);

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

  size_t length = (rxmax - rxmin) / res;
  size_t width  = (rymax - rymin) / res;
  size_t height = (rzmax - rzmin) / res;
  size_t nvoxels = length*width*height;

  if (nvoxels < INT_MAX/sizeof(std::vector<PointXYZ>)) // Cells numbers are stored has int
  {
    vregistry.resize(nvoxels);
    use_vregistry = true;
  }

  progress->reset();
  progress->set_prefix("Poisson disk sampling");
  progress->set_total(index.size());

  int n = 0;

  // Loop in a random order
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

    int vkey;
    Voxel ukey;

    // Do we retain this point? We will look into the 27 neighbors to find if it is not too close to an already inserted points
    bool valid = true;

    // Check the central voxel, this should be enough in most cases and allows to skip the 26 neighbors
    // >>>>>
    if (use_vregistry)
    {
      vkey = nx + ny*length + nz*length*width;

      for (const auto& neighbor : vregistry[vkey])
      {
        double dist_square = (px - neighbor.x) * (px - neighbor.x) +  (py - neighbor.y) * (py - neighbor.y) + (pz - neighbor.z) * (pz - neighbor.z);
        if (dist_square < r_square)
        {
          valid = false;
          break;
        }
      }
    }
    else
    {
      ukey = {nx, ny, nz};

      if (uregistry.find(ukey) != uregistry.end())
      {
        for (const auto& neighbor : uregistry[ukey])
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
    // >>>>>

    for (int dx = -1; dx <= 1 && valid; ++dx)
    {
      for (int dy = -1; dy <= 1 && valid; ++dy)
      {
        for (int dz = -1; dz <= 1; ++dz)
        {
          if (dx == 0 && dy == 0 && dz == 0) continue;

          // If there are points the voxel
          if (use_vregistry)
          {
            int key2 = (nx+dx) + (ny+dy)*length + (nz+dz)*length*width;

            if (key2 < 0 || key2 >= (int)vregistry.size()) continue; // This happens at the edges where the voxels are not allocated

            for (const auto& neighbor : vregistry[key2])
            {
              double dist_square = (px - neighbor.x) * (px - neighbor.x) +  (py - neighbor.y) * (py - neighbor.y) + (pz - neighbor.z) * (pz - neighbor.z);
              if (dist_square < r_square)
              {
                valid = false;
                break;
              }
            }
          }
          else
          {
            Voxel key2(nx+dx, ny+dy, nz+dz);

            if (uregistry.find(key2) != uregistry.end())
            {
              for (const auto& neighbor : uregistry[key2])
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
    }

    if (valid)
    {
      if (use_vregistry)
        vregistry[vkey].emplace_back(px, py, pz);
      else
        uregistry[ukey].emplace_back(px, py, pz);

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

  if (verbose) print(" sampling retained %d points\n", n);

  // In lasR, deleted points are not actually deleted. They are withhelded, skipped by each stage but kept
  // to avoid the cost of memory reallocation and memmove. Here, if we remove more than 33% of the points
  // actually remove the points. This will save some computation later.
  double ratio = (double)n/(double)las->npoints;
  if (ratio > 1/3)
  {
    if (!las->delete_withheld()) return false;
    if (verbose) print(" memory layout reallocated\n");
  }

  return true;
}

// VOXEL
bool LASRsamplingvoxels::set_parameters(const nlohmann::json& stage)
{
  res = stage.at("res");
  shuffle_size = stage.value("shuffle_size", 10000);
  return true;
}

bool LASRsamplingvoxels::process(LAS*& las)
{
  std::unordered_set<Voxel, VoxelHash> uregistry;
  std::vector<bool> bitregistry;
  bool use_bitregistry = false;

  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  shuffle(index, shuffle_size);

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
  int width  = (rymax - rymin)/res;
  int height = (rzmax - rzmin)/res;

  size_t nvoxels = (size_t)length*(size_t)width*(size_t)height;
  if (nvoxels < INT_MAX) // Cells numbers are stored has int
  {
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

    // Do we retain this point ? We look into the registry to know if the voxel exist. If not, we retain the point.
    if (use_bitregistry)
    {
       int key = nx + ny*length + nz*length*width;

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
      Voxel key(nx, ny, nz);

      if (uregistry.insert(key).second)
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

  if (verbose) print(" sampling retained %d points\n", n);

  // In lasR, deleted points are not actually deleted. They are withhelded, skipped by each stage but kept
  // to avoid the cost of memory reallocation and memmove. Here, if we remove more than 33% of the points
  // actually remove the points. This will save some computation later.
  double ratio = (double)n/(double)las->npoints;
  if (ratio > 1/3)
  {
    if (!las->delete_withheld()) return false;
    if (verbose) print(" memory layout reallocated\n");
  }

  return true;
}

// PIXEL
bool LASRsamplingpixels::set_parameters(const nlohmann::json& stage)
{
  res = stage.at("res");
  shuffle_size = stage.value("shuffle_size", 10000);
  return true;
}

bool LASRsamplingpixels::process(LAS*& las)
{
  std::unordered_set<int> uregistry;
  std::vector<bool> bitregistry;
  bool use_bitregistry = false;

  std::vector<int> index(las->npoints);
  std::iota(index.begin(), index.end(), 0);
  shuffle(index, shuffle_size);

  double rxmin = las->header->min_x;
  double rymin = las->header->min_y;
  double rxmax = las->header->max_x;
  double rymax = las->header->max_y;
  Grid grid(rxmin, rymin, rxmax, rymax, res);

  size_t npixels = grid.get_ncells();
  if (npixels < INT_MAX) // Cells numbers are stored has int
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
      if (uregistry.insert(key).second)
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

  if (verbose) print(" sampling retained %d points\n", n);

  // In lasR, deleted points are not actually deleted. They are withhelded, skipped by each stage but kept
  // to avoid the cost of memory reallocation and memmove. Here, if we remove more than 33% of the points
  // actually remove the points. This will save some computation later.
  double ratio = (double)n/(double)las->npoints;
  if (ratio > 1/3)
  {
    if (!las->delete_withheld()) return false;
    if (verbose) print(" memory layout reallocated\n");
  }

  return true;
}
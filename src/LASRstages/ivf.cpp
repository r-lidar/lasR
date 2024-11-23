#include "ivf.h"
#include "Grid.h"

#include <unordered_map>

bool LASRivf::process(LAS*& las)
{
  // Stores for a given voxel the number of point in its 27 voxels neighborhood
  std::unordered_map<Voxel, int, VoxelHash> uregistry;
  std::vector<int> vregistry;
  bool use_vregistry = false;

  double rxmin = las->newheader->min_x;
  double rymin = las->newheader->min_y;
  double rzmin = las->newheader->min_z;
  double rxmax = las->newheader->max_x;
  double rymax = las->newheader->max_y;
  double rzmax = las->newheader->max_z;

  rxmin = ROUNDANY(rxmin - 0.5 * res, res);
  rymin = ROUNDANY(rymin - 0.5 * res, res);
  rzmin = ROUNDANY(rzmin - 0.5 * res, res);
  rxmax = ROUNDANY(rxmax + 0.5 * res, res);
  rymax = ROUNDANY(rymax + 0.5 * res, res);
  rzmax = ROUNDANY(rzmax + 0.5 * res, res);

  int length = (rxmax - rxmin) / res;
  int width  = (rymax - rymin) / res;
  int height = (rzmax - rzmin) / res;
  size_t nvoxels = (size_t)length*(size_t)width*(size_t)height;

  if (!force_map && nvoxels < INT_MAX/sizeof(int)) // 256 MB
  {
    vregistry.resize(nvoxels);
    std::fill(vregistry.begin(), vregistry.end(), 0);
    use_vregistry = true;
  }

  progress->reset();
  progress->set_total(las->npoints*2);
  progress->set_prefix("Isolated voxels");

  Voxel ukey;
  int vkey;

  while (las->read_point())
  {
    int nx = std::floor((las->p.get_x() - rxmin) / res);
    int ny = std::floor((las->p.get_y() - rymin) / res);
    int nz = std::floor((las->p.get_z() - rzmin) / res);

    // Add one in the 27 neighboring voxels of this point
    for (int i : {-1,0,1})
    {
      for (int j : {-1,0,1})
      {
        for (int k : {-1,0,1})
        {
          if (i == 0 && j == 0 && k == 0)
            continue;

          int xi = nx+i;
          int yj = ny+j;
          int zk = nz+k;

          if (xi < 0 || xi >= length || yj < 0 || yj >= width || zk < 0 || zk >= height)
            continue; // This happens on the edges

          if (use_vregistry)
          {
            vkey = xi + yj*length + zk*length*width;
            vregistry[vkey]++;
          }
          else
          {
            ukey = {xi, yj, zk};
            uregistry[ukey]++;
          }
        }
      }
    }

    progress->update(las->current_point);
    if (progress->interrupted()) break;
  }

  AttributeHandler set_and_get_classification("Classification");

  // Loop again through each point.
  // Check if the number of points in its neighbourhood is above the threshold
  while (las->read_point())
  {
    int nx = std::floor((las->p.get_x() - rxmin) / res);
    int ny = std::floor((las->p.get_y() - rymin) / res);
    int nz = std::floor((las->p.get_z() - rzmin) / res);

    int count;

    if (use_vregistry)
    {
      vkey = nx + ny*length + nz*length*width;
      count = vregistry[vkey] ;
    }
    else
    {
      ukey = {nx, ny, nz};
      count = uregistry[ukey];
    }

    if (count < n)
    {
      set_and_get_classification(&las->p, classification);
    }

    progress->update(las->current_point + las->npoints);
    if (progress->interrupted()) break;
  }

  return true;
}

bool LASRivf::set_parameters(const nlohmann::json& stage)
{
  res = stage.value("res", 5.0);
  n = stage.value("n", 6);
  classification = stage.value("class", 18);
  force_map = stage.value("force_map", false);
  return true;
}

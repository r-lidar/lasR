#include "ivf.h"
#include "Grid.h"

#include <unordered_map>

bool LASRivf::process(PointCloud*& las)
{
  // 1. Setup Resolutions (Assuming these are members or passed in)
  // If your class only has 'res', you need to add res_y and res_z members.
  double res_x = this->res_x;
  double res_y = this->res_y;
  double res_z = this->res_z;

  std::unordered_map<Voxel, int, VoxelHash> uregistry;
  std::vector<int> vregistry;
  bool use_vregistry = false;

  double rxmin = las->header->min_x;
  double rymin = las->header->min_y;
  double rzmin = las->header->min_z;
  double rxmax = las->header->max_x;
  double rymax = las->header->max_y;
  double rzmax = las->header->max_z;

  // 2. Align Bounding Box: Use specific resolution for each axis
  rxmin = ROUNDANY(rxmin - 0.5 * res_x, res_x);
  rymin = ROUNDANY(rymin - 0.5 * res_y, res_y);
  rzmin = ROUNDANY(rzmin - 0.5 * res_z, res_z);
  rxmax = ROUNDANY(rxmax + 0.5 * res_x, res_x);
  rymax = ROUNDANY(rymax + 0.5 * res_y, res_y);
  rzmax = ROUNDANY(rzmax + 0.5 * res_z, res_z);

  // 3. Calculate Grid Dimensions
  // Ensure we don't divide by zero (safety check) and use floor/ceil correctly
  int length = (int)((rxmax - rxmin) / res_x);
  int width  = (int)((rymax - rymin) / res_y);
  int height = (int)((rzmax - rzmin) / res_z);

  // Safety clamp for empty dimensions (e.g. flat clouds)
  if (length < 1) length = 1;
  if (width < 1) width = 1;
  if (height < 1) height = 1;

  size_t nvoxels = (size_t)length * (size_t)width * (size_t)height;

  if (!force_map && nvoxels < INT_MAX/sizeof(int))
  {
    vregistry.resize(nvoxels);
    std::fill(vregistry.begin(), vregistry.end(), 0);
    use_vregistry = true;
  }

  progress->reset();
  progress->set_total(las->npoints*2);
  progress->set_prefix("Isolated voxels");

  Voxel ukey;
  // Use size_t for vkey to prevent overflow during calculation if grid is large
  size_t vkey;

  // Pre-calculate inverse resolution for speed (multiplication is faster than division)
  double inv_res_x = 1.0 / res_x;
  double inv_res_y = 1.0 / res_y;
  double inv_res_z = 1.0 / res_z;

  while (las->read_point())
  {
    // 4. Update Index Calculation
    int nx = std::floor((las->point.get_x() - rxmin) * inv_res_x);
    int ny = std::floor((las->point.get_y() - rymin) * inv_res_y);
    int nz = std::floor((las->point.get_z() - rzmin) * inv_res_z);

    for (int i : {-1,0,1})
    {
      for (int j : {-1,0,1})
      {
        for (int k : {-1,0,1})
        {
          if (i == 0 && j == 0 && k == 0) continue;

          int xi = nx + i;
          int yj = ny + j;
          int zk = nz + k;

          // Boundary checks
          if (xi < 0 || xi >= length || yj < 0 || yj >= width || zk < 0 || zk >= height)
            continue;

          if (use_vregistry)
          {
            // 5. Update flattening logic
            vkey = (size_t)xi + (size_t)yj * length + (size_t)zk * length * width;
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

  AttributeAccessor set_and_get_classification("Classification");

  // Second Pass
  while (las->read_point())
  {
    // Repeat index calculation
    int nx = std::floor((las->point.get_x() - rxmin) * inv_res_x);
    int ny = std::floor((las->point.get_y() - rymin) * inv_res_y);
    int nz = std::floor((las->point.get_z() - rzmin) * inv_res_z);

    int count;

    if (use_vregistry)
    {
      vkey = (size_t)nx + (size_t)ny * length + (size_t)nz * length * width;
      count = vregistry[vkey];
    }
    else
    {
      ukey = {nx, ny, nz};
      count = uregistry[ukey];
    }

    if (count < n)
    {
      set_and_get_classification(&las->point, classification);
    }

    progress->update(las->current_point + las->npoints);
    if (progress->interrupted()) break;
  }

  return true;
}

bool LASRivf::set_parameters(const nlohmann::json& stage)
{
  double base_res = stage.value("res", 5.0);
  res_x = stage.value("res_x", base_res);
  res_y = stage.value("res_y", base_res);
  res_z = stage.value("res_z", base_res);
  n = stage.value("n", 6);
  classification = stage.value("class", 18);
  force_map = stage.value("force_map", false);
  return true;
}

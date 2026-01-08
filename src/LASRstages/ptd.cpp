#include <vector>

#include "ptd.h"
#include "ptd/PTD.h"
#include "Profiler.h"

LASRptd::LASRptd() {}

bool LASRptd::set_parameters(const nlohmann::json& stage)
{
  this->max_iteration_distance = stage.value("distance", 1);
  this->max_iteration_angle = stage.value("angle", 30);
  this->seed_resolution_search = stage.value("res", 50);
  this->min_triangle_size = stage.value("min_size", 0.25);
  this->classification = stage.value("class", 2);
  this->max_iter = stage.value("max_iter", 50);
  this->buffer_size = stage.value("buffer_size", 30);
  return true;
}

bool LASRptd::process(PointCloud*& las)
{
  Profiler tot;

  // Compute some offset to geographic coordinate
  // and move close to (0,0) for floating point
  // accuracy
  double x_offset = (las->header->min_x + las->header->max_x)/2;
  double y_offset = (las->header->min_y + las->header->max_y)/2;
  double z_offset = (las->header->min_z + las->header->max_z)/2;
  size_t n = las->npoints;

  // =========================================
  // Querying potential ground points
  // =========================================

  // Keep only the lowest point per grid cell. This speeds
  // up considerably the number of point to process and help getting
  // a decent density of ground points

  Profiler prof;
  if (verbose) print(" Querying points of interest: ");

  double rxmin = las->header->min_x - x_offset;
  double rymin = las->header->min_y - y_offset;
  double rxmax = las->header->max_x - x_offset;
  double rymax = las->header->max_y - y_offset;
  Grid grid(rxmin, rymin, rxmax, rymax, min_triangle_size);
  size_t npixels = grid.get_ncells();
  std::vector<PTD::Point> candidates;
  candidates.resize(npixels);
  for(auto& p : candidates) { p.z = std::numeric_limits<double>::infinity(); }

  while (las->read_point())
  {
    if (pointfilter.filter(&las->point)) continue;

    double x = las->point.get_x() - x_offset;
    double y = las->point.get_y() - y_offset;
    double z = las->point.get_z() - z_offset;

    int cell = grid.cell_from_xy(x, y);
    if (cell < 0 || cell >= candidates.size()) continue;

    if (z < candidates[cell].z)
    {
      candidates[cell].x = x;
      candidates[cell].y = y;
      candidates[cell].z = z;
      candidates[cell].fid = las->current_point;
    }
  }

  auto new_end = std::remove_if(candidates.begin(), candidates.end(), [](const PTD::Point& p)
  {
    return p.z == std::numeric_limits<double>::infinity();
  });
  candidates.erase(new_end,  candidates.end());

  if (verbose) print("took %.2f secs\n", prof.elapsed());

  // =========================================
  // Progressive TIN densification
  // =========================================

  prof = Profiler();
  if (verbose) print(" Progressive tin densification: ");

  PTD::PTDParameters params;
  params.buffer_size = buffer_size;
  params.max_iter = max_iter;
  params.max_iteration_angle = max_iteration_angle;
  params.max_iteration_distance = max_iteration_angle;
  params.min_triangle_size = min_triangle_size;
  params.seed_resolution_search = seed_resolution_search;
  params.verbose = false;

  std::vector<unsigned int> gnd;
  std::vector<unsigned int> out;

  try
  {
    PTD::PTD ptd(params);
    ptd.run(candidates);
    gnd = ptd.get_ground_fid();
    out = ptd.get_spikes_fid();
  }
  catch(std::exception& e)
  {
    last_error = e.what();
    return false;
  }


  if (verbose) print("took %.2f secs\n", prof.elapsed());

  // =====================================
  // Reclassify the points as unclassified
  // =====================================

  prof = Profiler();
  if (verbose) print(" Classification: ");

  AttributeAccessor get_and_set_classification("Classification");

  // Reset classification to non classified
  while (las->read_point())
  {
    if (get_and_set_classification(&las->point) == classification)
      get_and_set_classification(&las->point, 1);
  }

  // =====================================
  // Classify ground points
  // =====================================

  for (auto fid : gnd)
  {
    las->seek(fid);
    get_and_set_classification(&las->point, classification);
  }

  for (auto fid : out)
  {
    las->seek(fid);
    get_and_set_classification(&las->point, 7);
  }

  if (verbose) print("took %.2f secs\n", prof.elapsed());

  return true;
}
#include <vector>

#include "ptd.h"
#include "ptd/PTD.h"
#include "Profiler.h"

LASRptd::LASRptd() {}

bool LASRptd::set_parameters(const nlohmann::json& stage)
{
  this->max_iteration_distance = stage.value("distance", 1.0f);
  this->max_iteration_angle = stage.value("angle", 30.0f);
  this->seed_resolution_search = stage.value("res", 50.0f);
  this->spacing = stage.value("spacing", 0.25f);
  this->classification = stage.value("class", 2);
  this->max_iter = stage.value("max_iter", 50);
  this->rotation = stage.value("rotation", 0.0f);
  this->buffer_size = stage.value("buffer_size", 30.0f);
  return true;
}

bool LASRptd::process(PointCloud*& las)
{
  Profiler tot;

  PTD::Parameters params;
  params.buffer_size = buffer_size;
  params.max_iter = max_iter;
  params.max_iteration_angle = max_iteration_angle;
  params.max_iteration_distance = max_iteration_distance;
  params.spacing = spacing;
  params.seed_resolution_search = seed_resolution_search;
  params.verbose = verbose;
  params.ncores = this->ncpu;

  PTD::Bbox bb;
  bb.xmin = las->header->min_x;
  bb.ymin = las->header->min_y;
  bb.zmin = las->header->min_z;
  bb.xmax = las->header->max_x;
  bb.ymax = las->header->max_y;
  bb.zmax = las->header->max_z;

  PTD::Logger logger = [](const std::string& msg) { print("%s\n", msg.c_str()); };

  std::vector<unsigned int> gnd;
  std::vector<unsigned int> out;

  try
  {
    PTD::PTD ptd(params, bb);
    ptd.set_logger(logger);

    while (las->read_point())
    {
      if (pointfilter.filter(&las->point)) continue;
      ptd.insert_point(las->point.get_x(), las->point.get_y(), las->point.get_z(), las->current_point);
    }

    ptd.run();

    gnd = ptd.get_ground_fid();
    out = ptd.get_spikes_fid();
  }
  catch(std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  // =====================================
  // Reclassify the points as unclassified
  // =====================================

  Profiler prof;

  AttributeAccessor get_and_set_classification("Classification");

  while (las->read_point())
  {
    if (get_and_set_classification(&las->point) == classification)
      get_and_set_classification(&las->point, 1);
  }

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

  if (verbose)
  {
    print("  Classification done in %.2f secs\n", prof.elapsed());
    print("  Total time %.2f secs\n", tot.elapsed());
  }

  return true;
}
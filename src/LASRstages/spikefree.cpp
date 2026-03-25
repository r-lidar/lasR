#include <vector>
#include <numeric>   // iota
#include <algorithm> // sort, min

#include "spikefree.h"
#include "spikefree/Spikefree.h"
#include "openmp.h"

LASRspikefree::LASRspikefree() {}

bool LASRspikefree::set_parameters(const nlohmann::json& stage)
{
  this->d_f = stage.value("freeze_distance", 0.5f);
  this->h_b = stage.value("height_buffer", 0.5f);
  this->res = stage.value("res", 1.0);
  raster = Raster(xmin, ymin, xmax, ymax, res, 1);
  return true;
}

bool LASRspikefree::process(PointCloud*& las)
{
  Spikefree::Parameters params;
  params.d_f = d_f;
  params.h_b = h_b;

  Spikefree::Bbox bb;
  bb.xmin = las->header->min_x;
  bb.ymin = las->header->min_y;
  bb.zmin = las->header->min_z;
  bb.xmax = las->header->max_x;
  bb.ymax = las->header->max_y;
  bb.zmax = las->header->min_z;

  Spikefree::Logger logger = [](const std::string& msg) { print("%s\n", msg.c_str()); };

  // Get the order in which to process points (Z decreasing)
  std::vector<size_t> idx(las->npoints);
  std::vector<float> z; z.reserve(las->npoints);
  while (las->read_point()) z.push_back(las->point.get_z());
  std::iota(idx.begin(), idx.end(), 0);
  std::sort(idx.begin(), idx.end(), [&](size_t i, size_t j) { return z[i] > z[j]; });

  try
  {
    Spikefree::Spikefree sf(params, bb);
    sf.set_logger(logger);

    while (las->read_point())
    {
      if (pointfilter.filter(&las->point)) continue;
      double x = las->point.get_x();
      double y = las->point.get_y();
      double z = las->point.get_z();
      sf.pre_insert_point(x,y,z);
    }

    progress->reset();
    progress->set_prefix("Spikefree");
    progress->set_total(idx.size());

    for (size_t i : idx)
    {
      (*progress)++;
      progress->show();

      las->seek(i);
      if (pointfilter.filter(&las->point)) continue;
      double x = las->point.get_x();
      double y = las->point.get_y();
      double z = las->point.get_z();

      sf.insert_point(x, y, z);
    }

    progress->done();

    // Rasterize
    progress->reset();
    progress->set_prefix("Rasterize");
    progress->set_total(raster.get_ncells());
    progress->set_ncpu(ncpu);

    // The next for loop is at the level a nested parallel region. Printing the progress bar
    // is not thread safe. We first check that we are in outer thread 0
    bool main_thread = omp_get_thread_num() == 0;

    #pragma omp parallel for num_threads(ncpu)
    for (int c = 0; c < raster.get_ncells(); ++c)
    {
      if (main_thread)
      {
        (*progress)++;
        progress->show();
      }

      double x = raster.x_from_cell(c);
      double y = raster.y_from_cell(c);
      double z = sf.get_z(x,y);
      if (std::isnan(z)) z = raster.get_nodata();
      raster.set_value(c, z);
    }

    progress->done();

  }
  catch(std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  return true;
}

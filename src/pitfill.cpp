#include "pitfill.h"
#include "geophoton/chm_prep.h"

LASRpitfill::LASRpitfill(double xmin, double ymin, double xmax, double ymax, int lap_size, float thr_lap, float thr_spk, int med_size, float dil_radius, LASRalgorithm* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  set_connection(algorithm);

  this->lap_size = lap_size;
  this->thr_lap = thr_lap;
  this->thr_spk = thr_spk;
  this->med_size = med_size;
  this->dil_radius = dil_radius;

  LASRalgorithmRaster* p = dynamic_cast<LASRalgorithmRaster*>(algorithm);
  if (p)
  {
    int buffer = MAX3(lap_size, med_size, dil_radius);
    p->get_raster().set_chunk_buffer(buffer); // Need the raster to be buffered. We can directly modify the raster from another algorithm since no data has been initialized.
    raster = Raster(p->get_raster());
  }
}

bool LASRpitfill::process(LAS*& las)
{
  if (connections.empty())
  {
    last_error = "Unitialized pointer to LASRalgorithm"; // # nocov
    return false; // # nocov
  }

  // 'connections' contains a single stage that is supposed to be a raster stage
  // This is the only supported stage as of feb 2024
  auto it = connections.begin();
  LASRalgorithmRaster* p = dynamic_cast<LASRalgorithmRaster*>(it->second);
  if (!p)
  {
    last_error = "Invalid pointer dynamic cast. Expecting a pointer to LASRalgorithmRaster"; // # nocov
    return false; // # nocov
  }

  const Raster& rin = p->get_raster();
  const auto geom = rin.get_data();
  int sncol = rin.get_ncols();
  int snlin = rin.get_nrows();

  int lap_size = 3;
  float thr_lap = 0.1;
  float thr_spk = -0.1;
  float med_size = 3;
  float dil_radius = 0;

  float* ans = geophoton::chm_prep(&geom[0], snlin, sncol, lap_size, thr_lap, thr_spk, med_size, dil_radius, rin.get_nodata());

  if (ans == NULL)
  {
    last_error = "st_onge::chm_prep failed to allocate memory"; // # nocov
    return false; // # nocov
  }

  for (int i = 0 ; i < snlin*sncol ; i++)
  {
    raster.set_value(i, ans[i]);
  }

  free(ans);

  return true;
}
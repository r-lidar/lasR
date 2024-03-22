#include "pitfill.h"
#include "geophoton/chm_prep.h"

LASRpitfill::LASRpitfill(double xmin, double ymin, double xmax, double ymax, int lap_size, float thr_lap, float thr_spk, int med_size, float dil_radius, LASRalgorithm* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->algorithm = algorithm;

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
  if (!algorithm)
  {
    last_error = "Unitialized pointer to LASRalgorithm"; // # nocov
    return false; // # nocov
  }

  LASRalgorithmRaster* p = dynamic_cast<LASRalgorithmRaster*>(algorithm);

  if (!p)
  {
    last_error = "Invalid pointer dynamic cast. Expecting a pointer to LASRalgorithmRaster"; // # nocov
    return false; // # nocov
  }

  const Raster& rin = p->get_raster();
  const auto geom = rin.get_data();
  int sncol = rin.get_ncols();
  int snlin = rin.get_nrows();

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

double LASRpitfill::need_buffer() const
{
  int buff = MAX3(lap_size, med_size, dil_radius);
  return buff*raster.get_xres();
}
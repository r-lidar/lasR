#include "loadraster.h"

LASRloadraster::LASRloadraster(double xmin, double ymin, double xmax, double ymax, const std::string& file, int band)
{
  this->ifile = file;
  this->band = band;

  raster.set_file(file);
  if (!raster.read_file())
    throw std::string("Failed to read raster file ") + file;

  const double (&bbox)[4]  = raster.get_full_extent();

  bool overlap =  !(xmin >= bbox[2] || xmax <= bbox[0] || ymin >= bbox[3] || ymax <= bbox[1]);

  if (!overlap)
    throw std::string("The raster and the point-cloud have non overlaping bounding boxes");
}

bool LASRloadraster::set_chunk(const Chunk& chunk)
{
  bool success;
  #pragma omp critical (load_raster)
  {
    success = raster.get_chunk(chunk, band);
  }
  return  success;
}
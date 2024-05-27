#include "loadraster.h"

LASRloadraster::LASRloadraster(const std::string& file, int band)
{
  this->ifile = file;
  this->band = band;

  raster.set_file(file);
  if (!raster.read_file())
    throw std::string("Failed to read raster file ") + file;
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
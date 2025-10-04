#include "loadraster.h"

bool LASRloadraster::set_parameters(const nlohmann::json& stage)
{
  ifile = stage.at("file");
  band = stage.value("band", 1);

  raster.set_file(ifile);
  if (!raster.read_file())
  {
    last_error = "failed to read raster file " + ifile;
    return false;
  }

  const double (&bbox)[4]  = raster.get_full_extent();

  bool overlap =  !(xmin >= bbox[2] || xmax <= bbox[0] || ymin >= bbox[3] || ymax <= bbox[1]);

  if (!overlap)
  {
    last_error = "the raster and the point-cloud have non overlaping bounding boxes";
    return false;
  }

  return true;
}

bool LASRloadraster::set_chunk(Chunk& chunk)
{
  bool success;
  #pragma omp critical (load_raster)
  {
    success = raster.get_chunk(chunk, band);
  }
  return  success;
}
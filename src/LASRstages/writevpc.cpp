#include "writevpc.h"

LASRvpcwriter::LASRvpcwriter(bool absolute_path, bool use_gpstime)
{
  this->absolute_path = absolute_path;
  this->use_gpstime = use_gpstime;
}

bool LASRvpcwriter::process(LAScatalog*& catalog)
{
  return catalog->write_vpc(ofile, crs, absolute_path, use_gpstime);
}
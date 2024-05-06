#include "writevpc.h"

LASRvpcwriter::LASRvpcwriter(bool absolute_path)
{
  this->absolute_path = absolute_path;
}

bool LASRvpcwriter::process(LAScatalog*& catalog)
{
  return catalog->write_vpc(ofile, crs, absolute_path);
}
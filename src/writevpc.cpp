#include "writevpc.h"

bool LASRvpcwriter::process(LAScatalog*& catalog)
{
  if (!catalog->write_vpc(ofile))
  {
    last_error = catalog->last_error;
    return false;
  }

  return true;
}
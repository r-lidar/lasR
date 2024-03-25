#include "writevpc.h"

bool LASRvpcwriter::process(LAScatalog*& catalog)
{
  return catalog->write_vpc(ofile);
}
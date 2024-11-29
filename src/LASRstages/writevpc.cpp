#include "writevpc.h"

bool LASRvpcwriter::set_parameters(const nlohmann::json& stage)
{
  absolute_path = stage.value("absolute", false);
  use_gpstime = stage.value("use_gpstime", false);
  return true;
}

bool LASRvpcwriter::process(Catalog*& catalog)
{
  return catalog->write_vpc(ofile, crs, absolute_path, use_gpstime);
}
#include "setcrs.h"

LASRsetcrs::LASRsetcrs()
{
  crs = CRS();
}

bool LASRsetcrs::set_parameters(const nlohmann::json& stage)
{
  int epsg = stage.value("epsg", 0);
  std::string wkt = stage.value("wkt", "");

  if (epsg > 0) crs = CRS(epsg, true);
  else if (wkt.size() > 0) crs = CRS(wkt, true);

  return true;
}

bool LASRsetcrs::process(Header*& header)
{
  header->crs = crs;
  return true;
}
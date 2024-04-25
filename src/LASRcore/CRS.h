#ifndef CRS_H
#define CRS_H

#include "error.h"
#include <string>
#include <gdal_priv.h>

class CRS
{
public:
  CRS();
  CRS(int, bool warn = false);
  CRS(const std::string&, bool warn = false);
  OGRSpatialReference get_crs() const;
  int get_epsg() const;
  std::string get_wkt() const;

private:
  int epsg;
  std::string wkt;
  OGRSpatialReference oSRS;
};

#endif
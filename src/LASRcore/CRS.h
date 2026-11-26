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
  bool is_valid() const;
  double get_linear_units() const;
  bool is_meters() const;
  bool is_feets() const;
  bool is_geographic() const;
  void dump() const;
  bool operator==(const CRS& other) const;
  bool is_same(const CRS& other) const; // the same CRS, possibly written with another WKT
  std::string get_wkt() const;

private:
  int epsg;
  bool valid;
  std::string wkt;
  OGRSpatialReference oSRS;
};

#endif
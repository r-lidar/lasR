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
  std::string get_wkt() const;

private:
  int epsg;
  bool valid;
  std::string wkt;
  OGRSpatialReference oSRS;
};

// Reproject an axis-aligned bounding box from `source` to `target` CRS. The boundary is
// densified before transforming so a curved (non-affine) reprojection is bounded correctly
// rather than only at the four corners. Returns false if the CRS are invalid, the
// transformation cannot be built, or no sample reprojects (the box is entirely outside the
// transform domain). An empty/unset box (min > max) is left unchanged and returns true.
bool reproject_bbox(const CRS& source, const CRS& target, double& xmin, double& ymin, double& xmax, double& ymax);

#endif
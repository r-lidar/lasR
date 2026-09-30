#ifndef CRS_H
#define CRS_H

#include "error.h"
#include <string>
#include <vector>
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
// densified before transforming, and the result is padded by an estimate of the curvature
// between samples, so the returned box contains the whole reprojected box and not only its
// corners. Returns false if the CRS are invalid, the transformation cannot be built, or no
// sample reprojects (the box is entirely outside the transform domain). An empty/unset box
// (min > max) is left unchanged and returns true.
// If `ring_x` and `ring_y` are given, they receive the reprojected boundary samples, in order
// around the box, or are left empty if a sample does not reproject.
bool reproject_bbox(const CRS& source, const CRS& target, double& xmin, double& ymin, double& xmax, double& ymax, std::vector<double>* ring_x = nullptr, std::vector<double>* ring_y = nullptr);

// Reproject the circle (xc, yc, r) of `source` CRS into `target` CRS. (cx, cy) receives the
// reprojected centre and `radius` the largest distance from it to the reprojected circle, with
// the same densification and padding as reproject_bbox(), so the circle (cx, cy, radius) contains
// the whole reprojected circle. `ring_x` and `ring_y` receive the reprojected samples of the
// circle as in reproject_bbox(). Returns false if the CRS are invalid, the transformation cannot
// be built, or the centre or every sample of the circle is outside the transform domain.
bool reproject_circle(const CRS& source, const CRS& target, double xc, double yc, double r, double& cx, double& cy, double& radius, std::vector<double>* ring_x = nullptr, std::vector<double>* ring_y = nullptr);

// The same with a coordinate transformation built by the caller. PROJ may choose a different
// coordinate operation (e.g. a grid shift or none) each time a transformation is built between
// the same CRS, depending on the thread. Using the transformation that reprojects the points
// keeps the extents consistent with the points.
bool reproject_bbox(OGRCoordinateTransformation* ct, double& xmin, double& ymin, double& xmax, double& ymax, std::vector<double>* ring_x = nullptr, std::vector<double>* ring_y = nullptr);
bool reproject_circle(OGRCoordinateTransformation* ct, double xc, double yc, double r, double& cx, double& cy, double& radius, std::vector<double>* ring_x = nullptr, std::vector<double>* ring_y = nullptr);

#endif

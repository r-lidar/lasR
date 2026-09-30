#include "CRS.h"
#include "print.h"

#include <ogr_spatialref.h>

#include <stdio.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

CRS::CRS()
{
  valid = false;
  epsg = 0;
}

CRS::CRS(int code, bool err)
{
  valid = false;
  epsg = code;
  if (epsg == 0) return;

  CPLPushErrorHandler(CPLQuietErrorHandler);

  if (oSRS.importFromEPSG(epsg) != OGRERR_NONE)
  {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "EPSG:%d %s\n", epsg, CPLGetLastErrorMsg());
    if (err) throw std::runtime_error(buffer);
    return;
  }

  valid = true;

  // Get wkt
  char *pszNewWKT;
  char **papszOptions = nullptr;
  papszOptions = CSLSetNameValue(papszOptions, "FORMAT", "WKT2");
  oSRS.exportToWkt(&pszNewWKT, papszOptions);
  wkt = std::string(pszNewWKT);

  CPLFree(pszNewWKT);
  CSLDestroy(papszOptions);

  CPLPopErrorHandler();
}

CRS::CRS(const std::string& str, bool err)
{
  valid = false;
  epsg = 0;
  wkt = str;
  if (wkt.empty()) return;

  CPLPushErrorHandler(CPLQuietErrorHandler);

  if (oSRS.importFromWkt(wkt.c_str()) != OGRERR_NONE)
  {
    char buffer[2048];
    snprintf(buffer, sizeof(buffer), "WKT string: %s", CPLGetLastErrorMsg());
    if (err) throw std::runtime_error(buffer);
    return;
  }

  valid = true;

  const char* authority_code = oSRS.GetAuthorityCode(NULL);
  if (authority_code != NULL)
  {
    epsg = std::stoi(authority_code);
  }

  CPLPopErrorHandler();
}

OGRSpatialReference CRS::get_crs() const
{
  return oSRS;
}

int CRS::get_epsg() const { return epsg; }
std::string CRS::get_wkt() const { return wkt; }
bool CRS::is_valid() const { return valid; }

double CRS::get_linear_units() const
{
  double dfLinearUnitSize;
  const char* pszLinearUnitName;
  dfLinearUnitSize = oSRS.GetLinearUnits(&pszLinearUnitName);
  return dfLinearUnitSize;
}

bool CRS::is_meters() const
{
  return get_linear_units() == 1.0f;
}

bool CRS::is_feets() const
{
  double value = get_linear_units();
  return std::fabs(value - 0.3048) < 1e-4;
}

bool CRS::is_geographic() const
{
  return valid && oSRS.IsGeographic();
}

bool CRS::operator==(const CRS& other) const
{
  return epsg == other.epsg && valid == other.valid && wkt == other.wkt;
}

// # nocov start
void CRS::dump() const
{

  int err = oSRS.Validate();
  if (err != OGRERR_NONE)
    print("Spatial reference is not valid: error %d\n", err);
  else
    print("Spatial reference is valid.\n");

  print("  EPSG: %d\n", epsg);
  print("  WKT: %s\n", wkt.substr(0,50).c_str());

  return;

  char* pszWKT = nullptr;
  oSRS.exportToPrettyWkt(&pszWKT);
  if (pszWKT)
  {
    print("WKT: %s\n", pszWKT);
    CPLFree(pszWKT);
  }
}

// # nocov end

static OGRCoordinateTransformation* create_transform(const CRS& source, const CRS& target)
{
  OGRSpatialReference oSourceSRS = source.get_crs();
  OGRSpatialReference oTargetSRS = target.get_crs();

  // Use traditional GIS axis order (x = lon/easting, y = lat/northing) so coordinates
  // are not swapped under modern PROJ authority-compliant axis ordering.
  oSourceSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
  oTargetSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

  CPLPushErrorHandler(CPLQuietErrorHandler);
  OGRCoordinateTransformation* ct = OGRCreateCoordinateTransformation(&oSourceSRS, &oTargetSRS);
  CPLPopErrorHandler();

  return ct;
}

// Transform the samples of a closed boundary and return the bounding box of the transformed
// boundary. The samples flagged 'coarse' (every other sample) describe the same boundary at half
// the density. Each side of the bounding box is moved outward by what doubling the density added
// on that side: the transformed boundary is curved between two samples, and this gain bounds what
// still lies beyond the samples (about a quarter of it for a smooth curve). Returns false if no
// sample reprojects. 'all_ok' tells whether every sample reprojected.
static bool transform_boundary_bbox(OGRCoordinateTransformation* ct, std::vector<double>& xs, std::vector<double>& ys, const std::vector<bool>& coarse, double& xmin, double& ymin, double& xmax, double& ymax, bool& all_ok)
{
  std::vector<int> ok(xs.size(), 0);
  ct->Transform((int)xs.size(), xs.data(), ys.data(), nullptr, ok.data());

  const double big = std::numeric_limits<double>::max();
  double fxmin = big, fymin = big, fxmax = -big, fymax = -big; // all the samples
  double cxmin = big, cymin = big, cxmax = -big, cymax = -big; // the coarse samples only
  bool any = false;
  bool any_coarse = false;
  all_ok = true;

  for (size_t i = 0; i < xs.size(); ++i)
  {
    if (!ok[i]) { all_ok = false; continue; }
    any = true;
    fxmin = std::min(fxmin, xs[i]);
    fymin = std::min(fymin, ys[i]);
    fxmax = std::max(fxmax, xs[i]);
    fymax = std::max(fymax, ys[i]);

    if (!coarse[i]) continue;
    any_coarse = true;
    cxmin = std::min(cxmin, xs[i]);
    cymin = std::min(cymin, ys[i]);
    cxmax = std::max(cxmax, xs[i]);
    cymax = std::max(cymax, ys[i]);
  }

  if (!any) return false;

  xmin = fxmin;
  ymin = fymin;
  xmax = fxmax;
  ymax = fymax;

  if (any_coarse)
  {
    xmin -= cxmin - fxmin;
    ymin -= cymin - fymin;
    xmax += fxmax - cxmax;
    ymax += fymax - cymax;
  }

  return true;
}

// Number of segments per edge (rectangle) or around the perimeter (circle). Must be even.
static const int NSEGMENTS = 64;

// a + (b - a) * i / n, exactly a for i = 0 and exactly b for i = n, so the neighbouring boxes that
// share an edge sample it at the exact same points.
static double lerp(double a, double b, int i, int n)
{
  if (i == 0) return a;
  if (i == n) return b;
  return a + (b - a) * i / n;
}

bool reproject_bbox(const CRS& source, const CRS& target, double& xmin, double& ymin, double& xmax, double& ymax, std::vector<double>* ring_x, std::vector<double>* ring_y)
{
  if (ring_x) ring_x->clear();
  if (ring_y) ring_y->clear();

  if (!source.is_valid() || !target.is_valid()) return false;

  // Nothing to do for an empty/unset extent.
  if (xmin > xmax || ymin > ymax) return true;

  OGRCoordinateTransformation* ct = create_transform(source, target);
  if (ct == nullptr) return false;

  bool success = reproject_bbox(ct, xmin, ymin, xmax, ymax, ring_x, ring_y);
  OGRCoordinateTransformation::DestroyCT(ct);
  return success;
}

bool reproject_bbox(OGRCoordinateTransformation* ct, double& xmin, double& ymin, double& xmax, double& ymax, std::vector<double>* ring_x, std::vector<double>* ring_y)
{
  if (ring_x) ring_x->clear();
  if (ring_y) ring_y->clear();

  if (ct == nullptr) return false;

  // Nothing to do for an empty/unset extent.
  if (xmin > xmax || ymin > ymax) return true;

  // Sample the boundary in order around the box, counterclockwise from (xmin, ymin). Every other
  // sample of each edge, corners included, is a coarse sample.
  const int N = NSEGMENTS;
  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<bool> coarse;
  xs.reserve(4 * N);
  ys.reserve(4 * N);
  coarse.reserve(4 * N);

  for (int i = 0; i < N; ++i) { xs.push_back(lerp(xmin, xmax, i, N)); ys.push_back(ymin); coarse.push_back(i % 2 == 0); } // bottom
  for (int i = 0; i < N; ++i) { xs.push_back(xmax); ys.push_back(lerp(ymin, ymax, i, N)); coarse.push_back(i % 2 == 0); } // right
  for (int i = N; i > 0; --i) { xs.push_back(lerp(xmin, xmax, i, N)); ys.push_back(ymax); coarse.push_back(i % 2 == 0); } // top
  for (int i = N; i > 0; --i) { xs.push_back(xmin); ys.push_back(lerp(ymin, ymax, i, N)); coarse.push_back(i % 2 == 0); } // left

  bool all_ok;
  bool success = transform_boundary_bbox(ct, xs, ys, coarse, xmin, ymin, xmax, ymax, all_ok);

  if (success && all_ok && ring_x && ring_y)
  {
    *ring_x = xs;
    *ring_y = ys;
  }

  return success;
}

bool reproject_circle(const CRS& source, const CRS& target, double xc, double yc, double r, double& cx, double& cy, double& radius, std::vector<double>* ring_x, std::vector<double>* ring_y)
{
  if (ring_x) ring_x->clear();
  if (ring_y) ring_y->clear();

  if (!source.is_valid() || !target.is_valid()) return false;
  if (!(r >= 0)) return false;

  OGRCoordinateTransformation* ct = create_transform(source, target);
  if (ct == nullptr) return false;

  bool success = reproject_circle(ct, xc, yc, r, cx, cy, radius, ring_x, ring_y);
  OGRCoordinateTransformation::DestroyCT(ct);
  return success;
}

bool reproject_circle(OGRCoordinateTransformation* ct, double xc, double yc, double r, double& cx, double& cy, double& radius, std::vector<double>* ring_x, std::vector<double>* ring_y)
{
  if (ring_x) ring_x->clear();
  if (ring_y) ring_y->clear();

  if (ct == nullptr) return false;
  if (!(r >= 0)) return false;

  // The centre first, then the samples of the circle, counterclockwise.
  const double pi = 3.14159265358979323846;
  const int N = NSEGMENTS;
  std::vector<double> xs(N + 1);
  std::vector<double> ys(N + 1);
  std::vector<int> ok(N + 1, 0);
  xs[0] = xc;
  ys[0] = yc;
  for (int i = 0; i < N; ++i)
  {
    double a = 2 * pi * i / N;
    xs[i+1] = xc + r * std::cos(a);
    ys[i+1] = yc + r * std::sin(a);
  }

  ct->Transform((int)xs.size(), xs.data(), ys.data(), nullptr, ok.data());

  if (!ok[0]) return false;
  cx = xs[0];
  cy = ys[0];

  // Largest distance from the centre to all the samples and to the coarse samples only. The
  // difference bounds what lies beyond the samples, as in transform_boundary_bbox().
  double rfine = -1;
  double rcoarse = -1;
  bool all_ok = true;
  for (int i = 0; i < N; ++i)
  {
    if (!ok[i+1]) { all_ok = false; continue; }
    double d = std::hypot(xs[i+1] - cx, ys[i+1] - cy);
    rfine = std::max(rfine, d);
    if (i % 2 == 0) rcoarse = std::max(rcoarse, d);
  }

  if (rfine < 0) return false;
  radius = rfine;
  if (rcoarse >= 0) radius += rfine - rcoarse;

  if (all_ok && ring_x && ring_y)
  {
    ring_x->assign(xs.begin() + 1, xs.end());
    ring_y->assign(ys.begin() + 1, ys.end());
  }

  return true;
}

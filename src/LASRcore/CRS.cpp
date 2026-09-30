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
// sample reprojects.
static bool transform_boundary_bbox(OGRCoordinateTransformation* ct, std::vector<double>& xs, std::vector<double>& ys, const std::vector<bool>& coarse, double& xmin, double& ymin, double& xmax, double& ymax)
{
  std::vector<int> ok(xs.size(), 0);
  ct->Transform((int)xs.size(), xs.data(), ys.data(), nullptr, ok.data());

  const double big = std::numeric_limits<double>::max();
  double fxmin = big, fymin = big, fxmax = -big, fymax = -big; // all the samples
  double cxmin = big, cymin = big, cxmax = -big, cymax = -big; // the coarse samples only
  bool any = false;
  bool any_coarse = false;

  for (size_t i = 0; i < xs.size(); ++i)
  {
    if (!ok[i]) continue;
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

bool reproject_bbox(const CRS& source, const CRS& target, double& xmin, double& ymin, double& xmax, double& ymax)
{
  if (!source.is_valid() || !target.is_valid()) return false;

  // Nothing to do for an empty/unset extent.
  if (xmin > xmax || ymin > ymax) return true;

  OGRCoordinateTransformation* ct = create_transform(source, target);
  if (ct == nullptr) return false;

  const int N = NSEGMENTS;
  std::vector<double> xs;
  std::vector<double> ys;
  std::vector<bool> coarse;
  xs.reserve(4 * (N + 1));
  ys.reserve(4 * (N + 1));
  coarse.reserve(4 * (N + 1));

  for (int i = 0; i <= N; ++i)
  {
    double tx = xmin + (xmax - xmin) * i / N;
    double ty = ymin + (ymax - ymin) * i / N;
    bool c = (i % 2 == 0);

    xs.push_back(tx);   ys.push_back(ymin); coarse.push_back(c); // bottom edge
    xs.push_back(tx);   ys.push_back(ymax); coarse.push_back(c); // top edge
    xs.push_back(xmin); ys.push_back(ty);   coarse.push_back(c); // left edge
    xs.push_back(xmax); ys.push_back(ty);   coarse.push_back(c); // right edge
  }

  bool success = transform_boundary_bbox(ct, xs, ys, coarse, xmin, ymin, xmax, ymax);
  OGRCoordinateTransformation::DestroyCT(ct);
  return success;
}

bool reproject_circle_bbox(const CRS& source, const CRS& target, double xc, double yc, double r, double& xmin, double& ymin, double& xmax, double& ymax)
{
  if (!source.is_valid() || !target.is_valid()) return false;
  if (!(r >= 0)) return false;

  OGRCoordinateTransformation* ct = create_transform(source, target);
  if (ct == nullptr) return false;

  const double pi = 3.14159265358979323846;
  const int N = NSEGMENTS;
  std::vector<double> xs(N);
  std::vector<double> ys(N);
  std::vector<bool> coarse(N);

  for (int i = 0; i < N; ++i)
  {
    double a = 2 * pi * i / N;
    xs[i] = xc + r * std::cos(a);
    ys[i] = yc + r * std::sin(a);
    coarse[i] = (i % 2 == 0);
  }

  bool success = transform_boundary_bbox(ct, xs, ys, coarse, xmin, ymin, xmax, ymax);
  OGRCoordinateTransformation::DestroyCT(ct);
  return success;
}

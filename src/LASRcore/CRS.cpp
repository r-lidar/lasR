#include "CRS.h"
#include "print.h"

#include <stdio.h>

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


#include "CRS.h"
#include "Rcompatibility.h"

CRS::CRS()
{
  epsg = 0;
}

CRS::CRS(int code, bool err)
{
  epsg = code;
  if (epsg == 0) return;

  CPLPushErrorHandler(CPLQuietErrorHandler);

  if (oSRS.importFromEPSG(epsg) != OGRERR_NONE && err)
  {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "EPSG:%d %s\n", epsg, CPLGetLastErrorMsg());
    last_error = std::string(buffer);
    throw last_error;
  }

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
  epsg = 0;
  wkt = str;
  if (wkt.empty()) return;

  CPLPushErrorHandler(CPLQuietErrorHandler);

  if (oSRS.importFromWkt(wkt.c_str()) != OGRERR_NONE && err)
  {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "WKT string: %s", CPLGetLastErrorMsg());
    last_error = std::string(buffer);
    throw last_error;
  }

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

int CRS::get_epsg() const
{
  return epsg;
}

std::string CRS::get_wkt() const
{
  return wkt;
}
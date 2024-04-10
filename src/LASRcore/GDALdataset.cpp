#include "GDALdataset.h"
#include "NA.h"

bool GDALdataset::initialized = false;

GDALdataset::GDALdataset()
{
  nBands = 0;
  nXsize = 0;
  nYsize = 0;
  geo_transform[0] = 0;
  geo_transform[1] = 0;
  geo_transform[2] = 0;
  geo_transform[3] = 0;
  geo_transform[4] = 0;
  geo_transform[5] = 0;

  dataset = nullptr;
  layer = nullptr;

  last_error_code = -1;

  nodata = NA_F32_RASTER;
  eGType = wkbUnknown;
  eType = GDT_Float32;
  dType = GDALDatasetType::UNDEFINED;
}

/*GDALdataset::GDALdataset(const GDALdataset& other)
{
  nBands = other.nBands;
  nXsize = other.nXsize;
  nYsize = other.nYsize;
  memcpy(geo_transform, other.geo_transform, 6*sizeof(double));

  dataset = nullptr;
  layer = nullptr;

  nodata = other.nodata;
  eGType = other.eGType;
  eType = other.eType;
  dType = other.dType;

  band_names = other.band_names;

  oSRS = other.oSRS;
}*/

bool GDALdataset::set_raster(double xmin, double ymax, int ncols, int nrows, double res)
{
  if (!check_dataset_is_not_initialized())
  {
    return false; // # nocov
  }

  geo_transform[0] = xmin;
  geo_transform[1] = res;
  geo_transform[2] = 0;
  geo_transform[3] = ymax;
  geo_transform[4] = 0;
  geo_transform[5] = -res;
  nXsize = ncols;
  nYsize = nrows;
  nBands = 1;
  eGType = wkbUnknown;
  band_names.resize(1);
  dType = GDALDatasetType::RASTER;

  return true;
}

bool GDALdataset::set_vector(OGRwkbGeometryType geometry_type)
{
  if (!check_dataset_is_not_initialized())
  {
    return false; // # nocov
  }

  nBands = 0;
  nXsize = 0;
  nYsize = 0;
  eGType = geometry_type;
  band_names.clear();
  dType = GDALDatasetType::VECTOR;

  return true;
}

bool GDALdataset::set_geometry_type(OGRwkbGeometryType geometry_type)
{
  if (!check_dataset_is_not_initialized())
  {
    return false; // # nocov
  }

  eGType = geometry_type;
  return true;
}

bool GDALdataset::set_nbands(int nbands)
{
  if (!check_dataset_is_not_initialized() && !check_dataset_is_raster())
  {
    return false; // # nocov
  }

  nBands = nbands;
  band_names.resize(nBands);
  return true;
}

bool GDALdataset::create_file()
{
  if (!check_dataset_is_not_initialized())
  {
    return false; // # nocov
  }

  if (file.empty())
  {
    last_error = "No filename"; // # nocov
    return false; // # nocov
  }

  if (dType == GDALDatasetType::UNDEFINED)
  {
    last_error = "GDALdataset must be either of type raster of vector"; // # nocov
    return false; // # nocov
  }

  initialize_gdal();

  GDALDriver* driver = nullptr;

  // Extract the driver name from the filename's extension
  size_t dotpos = file.find_last_of(".");
  if (dotpos == std::string::npos)
  {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "cannot find file extension for '%s'", file.c_str());
    last_error = std::string(buffer);
    return false;
  }

  std::string ext = file.substr(dotpos + 1);
  const auto it = extension2driver.find(ext);
  if (it == extension2driver.end())
  {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "file extension '%s' not registered in the database", ext.c_str());
    last_error = std::string(buffer);
    return false;
  }

  driver = GetGDALDriverManager()->GetDriverByName(it->second.c_str());

  if (!driver)
  {
    // # nocov start
    char buffer[64];
    snprintf(buffer, sizeof(buffer),  "no suitable GDAL driver found with name '%s'.", it->second.c_str());
    last_error = std::string(buffer);
    return false;
    // # nocov end
  }

  // Default compress option for GTiff driver
  char** papszOptions = NULL;
  if (strcmp(driver->GetDescription(), "GTiff") == 0)
  {
    papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "DEFLATE");
    papszOptions = CSLSetNameValue(papszOptions, "PREDICTOR", "2");
    papszOptions = CSLSetNameValue(papszOptions, "TILED", "YES");
  }

  dataset.reset(driver->Create(file.c_str(), nXsize, nYsize, nBands, eType, papszOptions), GDALClose);

  if (!dataset)
  {
    // # nocov start
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "error %d while creating GDAL dataset. %s", CPLGetLastErrorNo(),  CPLGetLastErrorMsg());
    last_error = std::string(buffer);
    return false;
    // # nocov end
  }

  if (is_raster())
  {
    dataset->SetSpatialRef(&oSRS);
    dataset->SetGeoTransform(geo_transform);

    for (int i = 1 ; i <= nBands ; ++i)
    {
      dataset->GetRasterBand(i)->SetNoDataValue(nodata);
      dataset->GetRasterBand(i)->SetDescription(band_names[i-1].c_str());
    }
  }
  else
  {
    if (eGType == wkbUnknown)
    {
      last_error = "no geometry type recorded for this GDALdataset"; // # nocov
      return false; // # nocov
    }

    layer = dataset->CreateLayer("layer", &oSRS, eGType, nullptr);

    if (layer == nullptr)
    {
      // # nocov start
      char buffer[512];
      snprintf(buffer, sizeof(buffer), "error %d while creating GDAL dataset layer. %s", CPLGetLastErrorNo(),  CPLGetLastErrorMsg());
      last_error = std::string(buffer);
      return false;
      // # nocov end
    }
  }

  return true;
}

bool GDALdataset::read_file()
{
  if (!check_dataset_is_not_initialized())
  {
    return false; // # nocov
  }

  initialize_gdal();

  // Open the raster dataset
  dataset.reset((GDALDataset*)GDALOpen(file.c_str(), GA_ReadOnly));
  if (dataset == nullptr)
  {
    last_error = "Error: Unable to open raster dataset.";
    return false;
  }

  // Get CRS information
  const char *wkt = dataset->GetProjectionRef();
  if (wkt != nullptr && strlen(wkt) > 0)
  {
    oSRS.importFromWkt(wkt);
  }

  // Check if the dataset if a raster or a vector
  nBands = dataset->GetRasterCount();
  if (nBands == 0)
  {
    last_error = "Only raster are supported in read mode";
    return false;
  }
  else
  {
    // Get raster information
    nXsize = dataset->GetRasterXSize();
    nYsize = dataset->GetRasterYSize();
    nBands = dataset->GetRasterCount();

    // Get geotransform information
    dataset->GetGeoTransform(geo_transform);

    dType = GDALDatasetType::RASTER;
  }

  return true;
}
bool GDALdataset::set_band_name(std::string name, int band)
{
  if (!check_dataset_is_raster())
  {
    return false; // # nocov
  }

  if (band >= nBands || band < 0)
  {
    last_error = "Invalid band number"; // # nocov
    return false; // # nocov
  }

  band_names[band] = name;

  if (dataset)
  {
    dataset->GetRasterBand(band+1)->SetDescription(band_names[band].c_str());
  }

  return true;
}

bool GDALdataset::set_crs(int epsg)
{
  if (oSRS.importFromEPSG(epsg) != OGRERR_NONE)
  {
    // # nocov start
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "error %d while creating GDAL dataset. %s", CPLGetLastErrorNo(),  CPLGetLastErrorMsg());
    last_error = std::string(buffer);
    return false;
    // # nocov end
  }

  return true;
}

bool GDALdataset::set_crs(std::string wkt)
{
  if (oSRS.importFromWkt(wkt.c_str()) != OGRERR_NONE)
  {
    // # nocov start
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "error %d while creating GDAL dataset. %s", CPLGetLastErrorNo(),  CPLGetLastErrorMsg());
    last_error = std::string(buffer);
    return false;
    // # nocov end
  }

  return true;
}

/*void GDALdataset::close()
{
  if (dataset)
  {
    GDALClose(dataset);
    dataset = nullptr;
  }
}*/

bool GDALdataset::check_dataset_is_not_initialized()
{
  if (dataset)
  {
    last_error = "Cannot perform the operation: the GDAL dataset is already initialized.";
    return false;
  }

  return true;
}

bool GDALdataset::check_dataset_is_raster()
{
  if (!is_raster())
  {
    last_error =  "Cannot perform the operation: the GDAL dataset is is not a raster";
    return false;
  }

  return true;
}

void GDALdataset::initialize_gdal()
{
  if (!initialized)
  {
    GDALAllRegister(); // Register GDAL drivers
    initialized = true; // Mark initialization as complete
  }
}

const std::map<std::string, std::string> GDALdataset::extension2driver = {
  {"bna", "BNA"}, // Vector
  {"csv", "CSV"},
  {"e00", "AVCE00"},
  {"fgb", "FlatGeobuf"},
  {"gdb", "OpenFileGDB"},
  {"geojson", "GeoJSON"},
  {"gml", "GML"},
  {"gmt", "GMT"},
  {"gpkg", "GPKG"},
  {"gps", "GPSBabel"},
  {"gpx", "GPX"},
  {"gtm", "GPSTrackMaker"},
  {"gxt", "Geoconcept"},
  {"jml", "JML"},
  {"kml", "KML"},
  {"map", "WAsP"},
  {"mdb", "Geomedia"},
  {"nc", "netCDF"},
  {"ods", "ODS"},
  {"osm", "OSM"},
  {"pbf", "OSM"},
  {"shp", "ESRI Shapefile"},
  {"sqlite", "SQLite"},
  {"vdv", "VDV"},
  {"xls", "xls"},
  {"xlsx", "XLSX"},
  {"tif", "GTiff"}, // Raster
  {"tiff", "GTiff"},
  {"jpg", "JPEG"},
  {"jpeg", "JPEG"},
  {"png", "PNG"},
  {"gif", "GIF"},
  {"bmp", "BMP"},
  {"jp2", "JPEG2000"},
  {"ecw", "ECW"},
  {"sid", "MrSID"},
  {"kml", "KML"},
  {"kmz", "KMZ"},
  {"gdb", "OpenFileGDB"},
  {"vrt", "VRT"},
  {"dxf", "DXF"}
};
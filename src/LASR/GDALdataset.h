#ifndef GDALDASET_H
#define GDALDASET_H

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

enum GDALDatasetType { UNDEFINED, RASTER, VECTOR };

class GDALdataset
{
public:
  GDALdataset();
  GDALdataset(const GDALdataset& other);
  ~GDALdataset();
  bool create_file();
  void set_file(std::string file) { this->file = file; }
  bool set_raster(double xmin, double ymax, int ncols, int nrows, double res);
  bool set_vector(OGRwkbGeometryType geometry_type);
  bool set_geometry_type(OGRwkbGeometryType geometry_type);
  bool set_nbands(int nbands);
  bool set_band_name(std::string name, int band);
  bool set_crs(int epsg);
  bool set_crs(std::string wkt);
  bool is_raster() { return dType == GDALDatasetType::RASTER; }
  bool is_vector() { return dType == GDALDatasetType::VECTOR; }
  void close();

  static const std::map<std::string, std::string> extension2driver;

protected:
  bool check_dataset_is_not_initialized();
  bool check_dataset_is_raster();

public:
  int nXsize;
  int nYsize;
  int nBands;
  int last_error_code;
  double geo_transform[6];

  float nodata;

  std::string file;
  std::string last_error;
  std::vector<std::string> band_names;

  GDALDatasetType dType;
  GDALDataType eType;
  GDALDataset* dataset;
  OGRLayer* layer;
  OGRSpatialReference oSRS;
  OGRwkbGeometryType eGType;

  enum warnings { DUPFID };
};

#endif
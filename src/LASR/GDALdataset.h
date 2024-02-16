#ifndef GDALDASET_H
#define GDALDASET_H

#include "error.h"

#include <memory>

#include <gdal_priv.h>
#include <ogrsf_frmts.h>

class GDALdataset
{
public:
  GDALdataset();
  GDALdataset(const GDALdataset& other);
  //~GDALdataset();
  bool create_file();
  void set_file(std::string file) { this->file = file; }
  bool set_raster(double xmin, double ymax, int ncols, int nrows, double res);
  bool set_vector(OGRwkbGeometryType geometry_type);
  bool set_geometry_type(OGRwkbGeometryType geometry_type);
  bool set_nbands(int nbands);
  bool set_band_name(std::string name, int band);
  bool set_crs(int epsg);
  bool set_crs(std::string wkt);
  bool is_raster() const { return dType == GDALDatasetType::RASTER; }
  bool is_vector() const { return dType == GDALDatasetType::VECTOR; }
  //void close();

  enum warnings { DUPFID };
  static const std::map<std::string, std::string> extension2driver;

protected:
  bool check_dataset_is_not_initialized();
  bool check_dataset_is_raster();

protected:
  enum GDALDatasetType { UNDEFINED, RASTER, VECTOR };

  int nXsize;
  int nYsize;
  int nBands;
  double geo_transform[6];

  float nodata;

  std::string file;
  std::vector<std::string> band_names;

  GDALDatasetType dType;    // The type of dataset (vector or raster or undefined)
  GDALDataType eType;       // The type of data in the dataset for rasters (is GDT_Float32)
  OGRSpatialReference oSRS;
  OGRwkbGeometryType eGType; // Type of geometry

  std::shared_ptr<GDALDataset> dataset; // Owner
  OGRLayer* layer;                      // Own by dataset
};

#endif
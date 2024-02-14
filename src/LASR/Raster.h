#ifndef RASTER_H
#define RASTER_H

#include "GDALdataset.h"
#include "Grid.h"

class Raster : public Grid, public GDALdataset
{
public:
  Raster();
  Raster(double xmin, double ymin, double xmax, double ymax, double res, int layers = 1);
  //Raster(double xmin, double ymin, double xmax, double ymax, int nrows, int ncols, int layers = 1);
  //Raster(const Raster& raster);
  Raster(const Raster& raster, double xmin, double ymin, double xmax, double ymax);
  void set_value(double x, double y, float value, int layer = 1);
  void set_value(int cell, float value, int layer = 1);
  bool set_nbands(int nbands);
  void set_chunk(double xmin, double ymin, double xmax, double ymax);
  void set_chunk_buffer(int buffer);
  int get_buffer() const { return buffer; };
  float get_nodata() const { return nodata; };
  float& get_value(double x, double y, int layer = 1);
  float& get_value(int cell, int layer = 1);
  const std::vector<float>& get_data() const { return data; };
  bool write();
  void show() const;
  float& operator()(int row, int col)
  {
    int cell = cell_from_row_col(row,col);
    return get_value(cell);
  }

private:
  int buffer;
  double extent[4];
  std::vector<float> data;
};


#endif //RASTER_H
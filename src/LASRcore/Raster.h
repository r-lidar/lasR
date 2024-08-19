#ifndef RASTER_H
#define RASTER_H

#include "GDALdataset.h"
#include "Grid.h"
#include "Chunk.h"

class Raster : public Grid, public GDALdataset
{
public:
  Raster();
  Raster(double xmin, double ymin, double xmax, double ymax, double res, int layers = 1);
  Raster(const Raster& raster);
  Raster(const Raster& raster, const Chunk& chunk);
  bool read_file();
  void set_value(double x, double y, float value, int layer = 1);
  void set_value(int cell, float value, int layer = 1);
  bool set_nbands(int nbands);
  void set_chunk(const Chunk& chunk);
  int get_nbands() const {return nBands; };
  int get_buffer() const { return buffer; };
  float get_nodata() const { return nodata; };
  float get_value(double x, double y, int layer = 1) const;
  float get_value(int cell, int layer = 1) const;
  bool get_chunk(const Chunk& chunk, int band_index);
  const std::vector<float>& get_data() const { return data; };
  bool copy_data(const Raster& raster);
  const double (&get_full_extent() const)[4] { return extent; };
  bool focal(float size, float (*operation)(std::vector<float>&));
  bool write();
  void show() const;
  float operator()(int row, int col, int layer = 1)
  {
    int cell = cell_from_row_col(row,col);
    return get_value(cell, layer);
  }

private:
  int buffer;
  bool circular;
  double extent[4]; // The actual full bbox of the underlying raster which is constant contrary to the grid bbox that correspond to the current chunk
  std::vector<float> data;
};


#endif //RASTER_H
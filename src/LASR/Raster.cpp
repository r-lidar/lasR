#include "Raster.h"
#include "NA.h"

#include "Rcompatibility.h"

#include <cmath>

// Default constructor creates a Raster from (0,0) to (0,0) with a resolution of 0
// GDALdataset is NOT initialized.
// data is NOT initialized
Raster::Raster() : Grid(0, 0, 0, 0, 0, 0), GDALdataset()
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  buffer = 0;

  GDALdataset::set_raster(0, 0, 0, 0, 0);
  nodata  = NA_F32_RASTER;
}

// Constructor from bounding box and resolution. Can assign a number of bands.
// GDALdataset is NOT initialized.
// data is NOT initialized
Raster::Raster(double xmin, double ymin, double xmax, double ymax, double res, int layers) : Grid (xmin, ymin, xmax, ymax, res), GDALdataset()
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  buffer = 0;

  GDALdataset::set_raster(this->xmin, this->ymax, this->ncols, this->nrows, this->xres);
  set_nbands(layers);
  nodata = NA_F32_RASTER;
}

// Copy constructor creates a Raster identical to the input but the underlying GDALDataset
// is not initialized and there is no file name associated to the new raster.
Raster::Raster(const Raster& raster) : Grid(raster), GDALdataset()
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  GDALdataset::set_raster(this->xmin, this->ymax, this->ncols, this->nrows, this->xres);
  buffer = raster.buffer;
  set_nbands(raster.nBands);
  band_names = raster.band_names;
  nodata = raster.nodata;
  oSRS = raster.oSRS;
}

// Copy constructor with bounding box. Creates a raster identical to the input
// (same number of bands, crs, band names, etc.) but with an extent that is different.
Raster::Raster(const Raster& raster, const Chunk& chunk) : Grid (chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax, raster.get_xres()), GDALdataset()
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  GDALdataset::set_raster(this->xmin, this->ymax, this->ncols, this->nrows, this->xres);
  set_nbands(raster.nBands);
  band_names = raster.band_names;
  nodata = raster.nodata;
  oSRS = raster.oSRS;

  set_chunk(chunk); // #15 resize the grid including the buffer
}

bool Raster::read_file()
{
  if (!GDALdataset::read_file())
  {
    return false;
  }

  if (!is_raster())
  {
    last_error = "This dataset is not a raster";
    return false;
  }

  // Multiple band reading not supported yet
  nBands = 1;

  // Compute Grid members
  ncols = nXsize;
  nrows = nYsize;
  ncells = ncols*nrows*ncells;
  xres = geo_transform[1];
  yres = geo_transform[5]*-1;
  xmin = geo_transform[0];
  ymax = geo_transform[3];
  xmax = xmin + nXsize * geo_transform[1];
  ymin = ymax + nYsize * geo_transform[5];

  // Compute Raster members
  extent[0] = xmin;
  extent[1] = ymin;
  extent[2] = xmax;
  extent[3] = ymax;

  return true;
}

float& Raster::get_value(double x, double y, int layer)
{
  int cell = cell_from_xy(x,y);
  return get_value(cell, layer);
}

float& Raster::get_value(int cell, int layer)
{
  if (data.size() == 0)
  {
    data.resize(nBands*ncells);
    std::fill(data.begin(), data.end(), nodata);
  }

  if (cell < ncells && cell >= 0)
    return data[cell + (layer-1)*ncells];
  else
    return nodata;
}

void Raster::set_value(double x, double y, float value, int layer)
{
  int cell = cell_from_xy(x,y);
  set_value(cell, value, layer);
}

void Raster::set_value(int cell, float value, int layer)
{
  if (data.size() == 0)
  {
    data.resize(nBands*ncells);
    std::fill(data.begin(), data.end(), nodata);
  }

  if (cell < ncells && cell >= 0)
    data[cell + (layer-1)*ncells] = value;
}

bool Raster::set_nbands(int nbands)
{
  if (!GDALdataset::set_nbands(nbands))
  {
    return false; // # nocov
  }

  if (data.size() > 0)
  {
    data.resize(nBands*ncells);
  }

  return true;
}

void Raster::set_chunk(const Chunk& chunk)
{
  buffer = std::ceil(chunk.buffer/xres); // buffer in pixel

  //print("Chunk %.1lf %.1lf %.1lf %.1lf (+%.1lf m)\n", chunk.xmin, chunk.xmax, chunk.ymin, chunk.ymax, chunk.buffer);

  // Adjust the grid to the chunk
  xmin = ROUNDANY(chunk.xmin - 0.5 * xres, xres) ;
  ymin = ROUNDANY(chunk.ymin - 0.5 * xres, xres) ;
  xmax = ROUNDANY(chunk.xmax - 0.5 * xres, xres) + xres;
  ymax = ROUNDANY(chunk.ymax - 0.5 * yres, yres) + yres;

  //print("Raster %.1lf %.1lf %.1lf %.1lf (+%d pix)\n", xmin, xmax, ymin, ymax, buffer);

  // Add a buffer
  xmin -= buffer*xres;
  ymin -= buffer*xres;
  xmax += buffer*xres;
  ymax += buffer*xres;

  //print("Raster %.1lf %.1lf %.1lf %.1lf (+%d pix)\n\n", xmin, xmax, ymin, ymax, buffer);

  ncols = std::round((xmax-xmin)/xres);
  nrows = std::round((ymax-ymin)/yres);
  ncells = ncols*nrows;

  data.clear();
}

bool Raster::get_chunk(const Chunk& chunk, int band_index)
{
  if (!dataset)
  {
    last_error = "Internal error: cannot get a raster chunk from uninitialized GDALDataset"; // nocov
    return false; // nocov
  }

  if (dataset->GetRasterCount() > band_index)
  {
    last_error = "Cannot read this band that is beyond the number of bands of this dataset";
    return false;
  }

  // Compute the size and number of cells of this chunk. Initialize the grid for this chunk.
  //double extended = buffer*xres; // buffer is given in pixel for a raster
  set_chunk(chunk);

  /*
   The query may be outside the actual raster on disk for two reasons:
    1. A buffer: is requested but we are on the edges of the raster.
    2. User error: and the point cloud is larger than the raster leading to a query outside the raster
       GDAL does not support querying a chunk that does not fit the underlying data. But in lasR
       we MUST NOT fail. Instead, everything that is beyond the raster must be loaded with NAs
   */
  double minx = this->xmin;
  double miny = this->ymin;
  double maxx = this->xmax;
  double maxy = this->ymax;
  if (minx < extent[0]) minx = extent[0];
  if (miny < extent[1]) miny = extent[1];
  if (maxx > extent[2]) maxx = extent[2];
  if (maxy > extent[3]) maxy = extent[3];
  int ncols = std::round((maxx-minx)/xres);
  int nrows = std::round((maxy-miny)/yres);
  int ncells = ncols*nrows;

  // Read raster data
  GDALRasterBand *band = dataset->GetRasterBand(band_index);
  eType = band->GetRasterDataType();
  nodata = band->GetNoDataValue();

  // We usually work only with a chunk. We need to compute the xy offsets considering that
  // we only have partial data in memory
  int xoffset = std::floor((minx - geo_transform[0]) / geo_transform[1]);
  int yoffset = std::floor((maxy - geo_transform[3]) / geo_transform[5]);

  // Read raster data
  void* gdal_buffer = CPLMalloc(ncells*sizeof(float));
  CPLErr err = band->RasterIO(GF_Read, xoffset, yoffset, ncols, nrows, gdal_buffer, ncols, nrows, GDT_Float32, 0, 0);

  if (err != CE_None)
  {
    last_error = std::string(CPLGetLastErrorMsg()); // # nocov
    return false;
  }

  // This is the grid corresponding the data actually read
  Grid gdal_grid(minx, miny, maxx, maxy, nrows, ncols);

  // Allocate memory for the actual raster data with potentially more cells than what was actually read
  data.resize(this->ncells);
  std::fill(data.begin(), data.end(), nodata);

  // Copy the data read from the raster to the actual chunk by celll
  for (int i = 0 ; i < ncols*nrows ; i++)
  {
    double x = gdal_grid.x_from_cell(i);
    double y = gdal_grid.y_from_cell(i);
    int cell = cell_from_xy(x,y);
    if (cell != -1) data[cell] = ((float*)gdal_buffer)[i];
  }

  CPLFree(gdal_buffer);

  return true;
}

bool Raster::write()
{
  if (data.size() == 0)
  {
    last_error = "cannot write with uninitialized data"; // # nocov
    return false; // # nocov
  }

  if (!dataset)
  {
    last_error = "cannot write with uninitialized GDALDataset"; // # nocov
    return false;
  }

  // We usually work only with a chunk. We need to compute the xy offsets considering that
  // we only have partial data in memory
  int xoffset = std::floor((xmin + buffer*xres - geo_transform[0]) / geo_transform[1]);
  int yoffset = std::floor((ymax - buffer*yres - geo_transform[3]) / geo_transform[5]);

  // Write the data to the raster band. If the raster is buffered we only write the main data
  // without the buffer
  CPLErr err;
  for (auto i = 1 ; i <= nBands ; ++i)
  {
    if (buffer == 0)
    {
      err = dataset->GetRasterBand(i)->RasterIO(GF_Write, xoffset, yoffset, ncols, nrows, &data[(i-1)*ncells], ncols, nrows, eType, 0, 0);
    }
    else
    {
      //printf("There is a buffer of size %d\n", buffer);
      int ncols_no_buffer = ncols - 2*buffer;
      int nrows_no_buffer = nrows - 2*buffer;
      std::vector<float> data_no_buffer(ncols_no_buffer*nrows_no_buffer);
      std::fill(data_no_buffer.begin(), data_no_buffer.end(), nodata);
      for (int row = buffer ; row < nrows - buffer ; ++row)
      {
        for (int col = buffer ; col < ncols - buffer ; ++col)
        {
          int originalIndex = row * ncols + col + (i-1)*ncells;
          int modifiedIndex = (row - buffer) * ncols_no_buffer + (col - buffer);
          data_no_buffer[modifiedIndex] = data[originalIndex];
        }
      }

      err = dataset->GetRasterBand(i)->RasterIO(GF_Write, xoffset, yoffset, ncols_no_buffer, nrows_no_buffer, &data_no_buffer[0], ncols_no_buffer, nrows_no_buffer, eType, 0, 0);
    }

    // Handle errors
    if (err != CE_None)
    {
      last_error = std::string(CPLGetLastErrorMsg()); // # nocov
      return false; // # nocov
    }
  }

  return true;
}

// # nocov start
void Raster::show() const
{
  print("class     : Raster\n");
  print("dimension : %d %d %d\n", nXsize, nYsize, nBands);
  print("chunk     : %d %d\n", ncols, nrows);
  print("resolution: %.2lf %.2lf\n", xres, yres);
  print("extent    : %.2lf %.2lf %.2lf %.2lf\n", xmin, ymin, xmax, ymax);
  print("filename  : %s\n", file.c_str());
  print("data init : %s\n", (data.size() == 0) ? "false" : "true");
}
// # nocov end
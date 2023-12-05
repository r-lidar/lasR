#include "Raster.h"
#include "NA.h"

#include "Rcompatibility.h"

Raster::Raster() : Grid(0, 0, 0, 0, 0, 0), GDALdataset()
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  buffer = 0;

  set_raster(0, 0, 0, 0, 0);
  nodata  = NA_F32_RASTER;
}

Raster::Raster(const Raster& raster) : Grid(raster), GDALdataset(raster)
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  buffer = raster.buffer;
  nodata = raster.nodata;
}

Raster::Raster(double xmin, double ymin, double xmax, double ymax, double res, int layers) : Grid (xmin, ymin, xmax, ymax, res), GDALdataset()
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  buffer = 0;

  set_raster(this->xmin, this->ymax, this->ncols, this->nrows, this->xres);
  set_nbands(layers);
  nodata = NA_F32_RASTER;
}

/*Raster::Raster(double xmin, double ymin, double xmax, double ymax, int nrows, int ncols, int layers) : Grid (xmin, ymin, xmax, ymax, nrows, ncols), GDALdataset()
{
  extent[0] = this->xmin;
  extent[1] = this->ymin;
  extent[2] = this->xmax;
  extent[3] = this->ymax;

  buffer = 0;

  set_raster(this->xmin, this->ymax, this->ncols, this->nrows, this->xres);
  set_nbands(layers);
  nodata  = NA_F32_RASTER;
}*/

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

void Raster::set_chunk(double xmin, double ymin, double xmax, double ymax)
{
  this->xmin = ROUNDANY(xmin - buffer*xres - 0.5 * xres, xres) ;
  this->ymin = ROUNDANY(ymin - buffer*yres - 0.5 * xres, xres) ;
  this->xmax = ROUNDANY(xmax + buffer*xres - 0.5 * xres, xres) + xres;
  this->ymax = ROUNDANY(ymax + buffer*yres - 0.5 * yres, yres) + yres;

  this->ncols = std::round((this->xmax-this->xmin)/xres);
  this->nrows = std::round((this->ymax-this->ymin)/yres);
  this->ncells = ncols*nrows;

  data.clear();
}

void Raster::set_chunk_buffer(int buffer)
{
  this->buffer = buffer;
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
    if (!create_file())
    {
      return false; // # nocov
    }
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
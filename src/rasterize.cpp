#include "rasterize.h"
#include "triangulate.h"

LASRrasterize::LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, double window, std::vector<int> methods)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;
  this->algorithm = nullptr;
  this->window = (window > res) ? (window-res)/2 : 0;

  raster = Raster(xmin, ymin, xmax, ymax, res, methods.size());

  for(size_t i = 0 ; i < methods.size() ; ++i)
  {
    switch (methods[i])
    {
    case 1:
      operators.push_back(&LASRrasterize::pmax);
      raster.set_band_name("max", i);
      break;
    case 2:
      operators.push_back(&LASRrasterize::pmin);
      raster.set_band_name("min", i);
      break;
    case 3:
      operators.push_back(&LASRrasterize::pcount);
      raster.set_band_name("count", i);
      break;
    default:
      break;
    }
  }
}

LASRrasterize::LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, LASRalgorithm* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;
  this->algorithm = algorithm;
  this->window = 0;

  raster = Raster(xmin, ymin, xmax, ymax, res, 1);
}

bool LASRrasterize::process(LASpoint*& p)
{
  // No operator registered means that we are in a non streamable rasterization
  if (operators.size() == 0) return true;
  if (lasfilter.filter(p)) return true;

  double x = p->get_x();
  double y = p->get_y();
  double z = p->get_z();

  std::vector<int> cells;
  if (window)
    raster.get_cells(x-window,y-window, x+window,y+window, cells);
  else
    cells.push_back(raster.cell_from_xy(x,y));

  for (size_t i = 0 ; i < operators.size() ; ++i)
  {
    for (int cell : cells)
    {
      float v = raster.get_value(cell, i+1);
      FunctionPointer& f = operators[i];
      raster.set_value(cell, (this->*f)(v, z), i+1);
    }
  }

  return true;
}

bool LASRrasterize::process(LAS*& las)
{
  if (!algorithm)
  {
    LASpoint* p;
    while (las->read_point())
    {
      p = &las->point;
      if (!process(p))
        return false; // # nocov
    }
    return true;
  }

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(algorithm);
  if (!p)
  {
    last_error  = "invalid pointer. Expecting a pointer to LASRtriangulate (internal error, please report)"; // # nocov
    return false; // # nocov
  }

  std::vector<double> z;
  p->interpolate(z, &raster);
  for (auto i = 0 ; i < raster.get_ncells() ; i++) raster.set_value(i, (float)z[i]);

  return true;
}
#include "rasterize.h"
#include "triangulate.h"
#include "Grouper.h"

LASRrasterize::LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, double window, const std::vector<std::string>& methods)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;
  this->window = (window > res) ? (window-res)/2 : 0;
  raster = Raster(xmin, ymin, xmax, ymax, res, methods.size());

  metrics.parse(methods);
  for (int j = 0 ; j < metrics.size() ; j++)
    raster.set_band_name(methods[j], j);

  this->streamable = metrics.is_streamable();
}

LASRrasterize::LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, LASRalgorithm* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;
  this->window = 0;
  this->streamable = false;

  set_connection(algorithm);

  raster = Raster(xmin, ymin, xmax, ymax, res, 1);
}

bool LASRrasterize::process(LASpoint*& p)
{
  // No operator registered means that we are in a non streamable rasterization
  if (metrics.size() == 0) return true;
  if (lasfilter.filter(p)) return true;

  double x = p->get_x();
  double y = p->get_y();
  double z = p->get_z();

  std::vector<int> cells;
  if (window)
    raster.get_cells(x-window,y-window, x+window,y+window, cells);
  else
    cells.push_back(raster.cell_from_xy(x,y));

  for (int i = 0 ; i < metrics.size() ; ++i)
  {
    for (int cell : cells)
    {
      float v = raster.get_value(cell, i+1);
      float res = metrics.get_metric(i, v, z);
      raster.set_value(cell, res, i+1);
    }
  }

  return true;
}

bool LASRrasterize::process(LAS*& las)
{
  // Streamable metrics:
  // but we are in a non streamble pipeline. We can call streamable code
  if (streamable)
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

  // We have a connection to LASRtriangulate:
  // we want to rasterize a triangulation mesh
  if (connections.size() > 0)
  {
    auto it = connections.begin();
    LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(it->second);
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

  // Last option:
  // we rasterize metrics that are not streamable  (code partially from aggregate)
  Grouper grouper;
  std::vector<int> cells;
  while (las->read_point(true)) // Need to include withheld points to do not mess grouper indexes
  {
    if (lasfilter.filter(&las->point)) continue; // TODO: check why and if we need that one

    double x = las->point.get_x();
    double y = las->point.get_y();

    if (window)
      raster.get_cells(x-window,y-window, x+window,y+window, cells);
    else
      cells.push_back(raster.cell_from_xy(x,y));

    grouper.insert(cells);
    cells.clear();
  }

  // Loop through each group on which we want to apply the call
  auto map = grouper.map;
  for (const auto& pair : map)
  {
    int cell = pair.first;
    las->set_intervals_to_read(pair.second);

    // Read the points of the query and populate the list
    while (las->read_point())
    {
      if (lasfilter.filter(&las->point))
        continue;

      metrics.add_point(&las->point);
    }

    for (int i = 0 ; i < metrics.size() ; i++)
    {
      float val = metrics.get_metric(i);
      raster.set_value(cell, val, i+1);
    }

    metrics.reset();
  }

  return true;
}
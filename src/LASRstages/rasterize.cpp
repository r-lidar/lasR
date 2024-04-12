#include "rasterize.h"
#include "triangulate.h"
#include "Grouper.h"
#include "openmp.h"

LASRrasterize::LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, double window, const std::vector<std::string>& methods)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;
  this->window = (window > res) ? (window-res)/2 : 0;
  raster = Raster(xmin, ymin, xmax, ymax, res, methods.size());

  if (!metrics.parse(methods))
    throw last_error;

  for (int j = 0 ; j < metrics.size() ; j++)
    raster.set_band_name(methods[j], j);

  this->streamable = metrics.is_streamable();
}

LASRrasterize::LASRrasterize(double xmin, double ymin, double xmax, double ymax, double res, Stage* algorithm)
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
  if (p->get_withheld_flag() != 0) return true;
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

  progress->reset();
  progress->set_prefix("Rasterize");
  progress->set_total(map.size());
  progress->show();

  // OpenMP cannot parallelize on a map. Create vectors to hold keys and references to corresponding vectors
  size_t n = grouper.map.size();
  std::vector<int> keys;
  std::vector<std::vector<Interval>*> intervals;
  keys.reserve(n);
  intervals.reserve(n);
  for (auto& pair : grouper.map)
  {
    keys.push_back(pair.first);
    intervals.push_back(&pair.second);
  }

  progress->reset();
  progress->set_total(n);
  progress->set_prefix("Rasterization");

  // The next for loop is at the level 2 of a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  // Protect against data race. The first call initialize the memory and is not thread safe
  // Next calls, all touch a different cell and are thus thread safe
  raster.set_value(0, NA_F32_RASTER, 1);

  #pragma omp parallel for num_threads(ncpu) firstprivate(metrics)
  for (size_t i = 0; i < n; ++i)
  {
    std::vector<PointLAS> pts;
    int cell = keys[i];
    las->query(*intervals[i], pts, &lasfilter);

    for (const auto& p : pts) metrics.add_point(p);

    for (int i = 0 ; i < metrics.size() ; i++)
    {
      float val = metrics.get_metric(i);
      raster.set_value(cell, val, i+1);
    }

    metrics.reset();

    if (main_thread)
    {
      #pragma omp critical
      {
        // can only be called in outer thread 0 AND is internally thread safe being called only in outer thread 0
        (*progress)++;
        progress->show();
      }
    }
  }

  progress->done();

  return true;
}
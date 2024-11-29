#include "rasterize.h"
#include "triangulate.h"
#include "Grouper.h"
#include "openmp.h"

bool LASRrasterize::set_parameters(const nlohmann::json& stage)
{
  double res = stage.at("res");

  if (connections.size() == 0)
  {
    methods = get_vector<std::string>(stage["method"]);

    window = stage.value("window", res);
    window = (window > res) ? (window-res)/2 : 0;
    raster = Raster(xmin, ymin, xmax, ymax, res, methods.size());

    if (!metric_engine.parse(methods))
    {
      return false;
    }

    float default_value = stage.value("default_value", NA_F32_RASTER);
    metric_engine.set_default_value(default_value);

    for (int j = 0 ; j < metric_engine.size() ; j++)
      raster.set_band_name(methods[j], j);

    streamable = metric_engine.is_streamable();
  }
  else
  {
    window = 0;
    streamable = false;
    raster = Raster(xmin, ymin, xmax, ymax, res, 1);
  }

  return true;
}

bool LASRrasterize::process(Point*& p)
{
  if (!metric_engine.is_streamable())  return true;
  if (p->get_deleted() != 0) return true;
  if (pointfilter.filter(p)) return true;

  double x = p->get_x();
  double y = p->get_y();
  double z = p->get_z();

  std::vector<int> cells;
  if (window)
    raster.get_cells(x-window,y-window, x+window,y+window, cells);
  else
    cells.push_back(raster.cell_from_xy(x,y));

  for (int i = 0 ; i < metric_engine.size() ; ++i)
  {
    for (int cell : cells)
    {
      float v = raster.get_value(cell, i+1);
      float res = metric_engine.get_metric(i, v, z);
      raster.set_value(cell, res, i+1);
    }
  }

  return true;
}

bool LASRrasterize::process(PointCloud*& las)
{
  // Streamable metrics:
  // but we are in a non streamble pipeline. We can call streamable code
  if (streamable)
  {
    Point* p;
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
    if (!p->interpolate(z, &raster)) return false;
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
  progress->set_ncpu(ncpu);
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
  progress->set_ncpu(ncpu);

  // The next for loop is at the level 2 of a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  // Protect against data race. The first call initialize the memory and is not thread safe
  // Next calls, all touch a different cell and are thus thread safe
  raster.set_value(0, NA_F32_RASTER, 1);

  #pragma omp parallel for num_threads(ncpu) firstprivate(metric_engine)
  for (size_t i = 0; i < n; ++i)
  {
    if (progress->interrupted()) continue;

    std::vector<Point> pts;
    int cell = keys[i];
    las->query(*intervals[i], pts, &pointfilter);

    for (int i = 0 ; i < metric_engine.size() ; i++)
    {
      float val = metric_engine.get_metric(i, pts);
      raster.set_value(cell, val, i+1);
    }

    if (main_thread)
    {
      #pragma omp critical
      {
        // can only be called in outer thread 0 AND is internally thread safe being called only in inner thread 0
        (*progress)++;
        progress->show();
      }
    }
  }

  progress->done();

  return true;
}

bool LASRrasterize::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  LASRtriangulate* p = dynamic_cast<LASRtriangulate*>(s);

  if (p)
    set_connection(p);
  else
  {
    last_error = "Incompatible stage combination for 'rasterize'"; // # nocov
    return false; // # nocov
  }

  return true;
}
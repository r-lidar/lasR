#include "openmp.h"
#include "ipf.h"

bool LASRipf::process(PointCloud*& las)
{
  progress->reset();
  progress->set_total(las->npoints);
  progress->set_prefix("Statistical outlier");
  progress->set_ncpu(ncpu);

  std::vector<int> n_neighbors;
  n_neighbors.resize(las->npoints);

  // The next for loop is at the level a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  if (verbose) print("Building KDtree spatial index\n");
  las->build_kdtree();

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < las->npoints ; i++)
  {
    if (progress->interrupted()) continue;

    Point p;
    p.set_schema(&las->header->schema);

    if (!las->get_point(i, &p)) continue;

    std::vector<Point> pts;
    las->query_sphere(p, radius, pts, &pointfilter);

    n_neighbors[i] = pts.size();

    #pragma omp critical (ipf)
    {
      if (main_thread)
      {
        (*progress)++;
        progress->show();
      }
    }
  }

  AttributeAccessor set_classification("Classification");

  while (las->read_point())
  {
    int i = las->current_point;

    if (n_neighbors[i] <= n)
    {
      set_classification(&las->point, classification);
    }
  }

  return true;
}

bool LASRipf::set_parameters(const nlohmann::json& stage)
{
  radius = stage.value("radius", 1.0);
  n = stage.value("n", 0);
  n++;
  classification = stage.value("class", 18);

  if (radius <= 0)
  {
    last_error = "Parameter 'radius' should be positive.";
    return false;
  }

  if (n < 0)
  {
    last_error = "Parameter 'n' should be positive or 0.";
    return false;
  }

  return true;
}

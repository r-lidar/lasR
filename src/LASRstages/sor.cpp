#include "sor.h"
#include "Grid.h"

bool LASRsor::process(LAS*& las)
{
  progress->reset();
  progress->set_total(las->npoints);
  progress->set_prefix("Statistical outlier");
  progress->set_ncpu(ncpu);

  int n = 0; // online variance
  double m0 = 0.0; // online mean
  double m2 = 0.0;
  std::vector<double> distances;
  distances.reserve(las->npoints);

  #pragma omp parallel for num_threads(ncpu)
  for (unsigned int i = 0 ; i < las->npoints ; i++)
  {
    (*progress)++;
    if (progress->interrupted()) continue;

    Point p;
    p.set_schema(&las->header->schema);

    if (!las->get_point(i, &p)) continue;

    std::vector<Point> pts;
    las->knn(p, k+1, std::numeric_limits<double>::max(), pts, &pointfilter);

    double dsum = 0;
    for (size_t i = 1; i < pts.size(); ++i) dsum += std::sqrt(std::pow(p.get_x() - pts[i].get_x(), 2) + std::pow(p.get_y() - pts[i].get_y(), 2) + std::pow(p.get_z() - pts[i].get_z(), 2));
    double dmean =  dsum / (pts.size()-1);
    distances[i] = dmean;

    #pragma omp critical (sor)
    {
      // Average distance and variance
      n++;
      double delta = dmean - m0;
      m0 += delta/n;
      m2 += delta*(dmean - m0);
    }
  }

  double dmean = m0;
  double dstd = std::sqrt(m2/(n-1));

  AttributeAccessor set_classification("Classification");

  while (las->read_point())
  {
    int i = las->current_point;

    if (distances[i] > dmean + m*dstd)
    {
      set_classification(&las->point, classification);
    }
  }

  return true;
}

bool LASRsor::set_parameters(const nlohmann::json& stage)
{
  k = stage.value("k", 10);
  m = stage.value("m", 3);
  classification = stage.value("class", 18);

  if (k < 2)
  {
    last_error = "Impossible to compute standard deviation with less than 2-nearest neighbors.";
    return false;
  }

  return true;
}

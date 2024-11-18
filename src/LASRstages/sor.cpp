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

    PointLAS p;
    if (!las->get_point(i, p)) continue;

    std::vector<PointLAS> pts;
    double xyz[3] = {p.x, p.y, p.z};
    las->knn(xyz, k+1, F64_MAX, pts, &lasfilter);

    double dsum = 0;
    for (size_t i = 1; i < pts.size(); ++i) dsum += std::sqrt(std::pow(p.x - pts[i].x, 2) + std::pow(p.y - pts[i].y, 2) + std::pow(p.z - pts[i].z, 2));
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

  while (las->read_point())
  {
    int i = las->current_point;

    if (distances[i] > dmean + m*dstd)
    {
      las->point.set_classification(classification);
      las->update_point();
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

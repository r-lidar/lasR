#include "nnmetrics.h"
#include "localmaximum.h"
#include "openmp.h"
#include "error.h"

#include <vector>
#include <iostream>

#define PUREKNN 0
#define KNNRADIUS 1
#define PURERADIUS 2

LASRnnmetrics::LASRnnmetrics(double xmin, double ymin, double xmax, double ymax, int k, double r, const std::vector<std::string>& methods, Stage* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  this->k = k;
  this->r = r;

  if (k == 0 && r > 0) mode = PURERADIUS;
  else if (k > 0 && r == 0) mode = PUREKNN;
  else if (k > 0 && r > 0) mode = KNNRADIUS;
  else throw "Internal error: invalid argument k or r"; // # nocov

  if (mode == PUREKNN) this->r = F64_MAX;

  if (!metrics.parse(methods)) throw last_error;

  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPoint25D);
  for (const auto& attr : methods) vector.add_field(attr, OFTReal);

  set_connection(algorithm);
}

bool LASRnnmetrics::process(LAS*& las)
{
  // Get the maxima from the local maximum stage
  auto it = connections.begin();
  LASRlocalmaximum* lmx = dynamic_cast<LASRlocalmaximum*>(it->second);
  if (lmx == nullptr)
  {
    last_error = "invalid dynamic cast. Expecting a pointer to LASRlocalmaximum"; // # nocov
    return false; // # nocov
  }
  const auto& maxima = lmx->get_maxima();

  // The next for loop is at the level 1 of a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  progress->reset();
  progress->set_total(maxima.size());
  progress->set_prefix("neighborhood_metrics");
  progress->show();

  #pragma omp parallel for num_threads(ncpu) firstprivate(metrics)
  for (int i = 0 ; i < maxima.size() ; i++)
  {
    if (progress->interrupted()) continue;

    const PointLAS& p = maxima[i];

    std::vector<PointLAS> pts;
    if (mode == PURERADIUS)
    {
      Sphere s(p.x, p.y, p.z, r);
      las->query(&s, pts, &lasfilter);
    }
    else
    {
      double xyz[3] = {p.x, p.y, p.z};
      las->knn(xyz, k, r, pts, &lasfilter);
    }

    PointXYZAttrs pt(p.x, p.y, p.z);
    pt.vals.reserve(metrics.size());
    for (int i = 0 ; i < metrics.size() ; i++)
    {
      float val = metrics.get_metric(i, pts);
      pt.vals.push_back(val);
    }
    lm.push_back(pt);

    #pragma omp critical
    {
      if (main_thread)
      {
        (*progress)++;
        progress->show();
      }
    }
  }

  progress->done();

  return true;
}

bool LASRnnmetrics::write()
{
  if (ofile.empty()) return true;
  if (lm.size() == 0) return true;

  progress->reset();
  progress->set_total(lm.size());
  progress->set_prefix("Write neighborhood metrics on disk");

  for (const auto& p : lm)
  {
    bool success;
    #pragma omp critical (write_nnmetric)
    {
      success = vector.write(p);
    }

    if (!success) return false;

    (*progress)++;
    progress->show();
  }

  return true;
}
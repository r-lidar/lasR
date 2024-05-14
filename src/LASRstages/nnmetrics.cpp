#include "nnmetrics.h"
#include "localmaximum.h"
#include "openmp.h"
#include "error.h"

#include <vector>
#include <iostream>

#define PUREKNN 0
#define KNNRADIUS 1
#define PURERADIUS 2

LASRnnmetrics::LASRnnmetrics(int k, double r, std::string features, const std::vector<std::string>& methods, Stage* algorithm)
{
  this->k = k;
  this->r = r;

  if (k == 0 && r > 0) mode = PURERADIUS;
  else if (k > 0 && r == 0) mode = PUREKNN;
  else if (k > 0 && r > 0) mode = KNNRADIUS;
  else throw "Internal error: invalid argument k or r"; // # nocov

  if (mode == PUREKNN) this->r = F64_MAX;

  set_connection(algorithm);
}

bool LASRnnmetrics::process(LAS*& las)
{
  progress->reset();
  progress->set_total(las->npoints);
  progress->set_prefix("neighborhood_metrics");
  progress->show();

  auto it = connections.begin();
  LASRlocalmaximum* lmx = dynamic_cast<LASRlocalmaximum*>(it->second);

  if (lmx == nullptr)
  {
    last_error = "invalid dynamic cast. Expecting a pointer to LASRlocalmaximum"; // # nocov
    return false; // # nocov
  }

  // The next for loop is at the level a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  #pragma omp parallel for num_threads(ncpu)
  for (int i = 0 ; i < lmx->get_maxima().size() ; i++)
  {
    if (progress->interrupted()) continue;

    std::vector<PointLAS> pts;
    double xyz[3];
    las->get_xyz(i, xyz);

    if (mode == PURERADIUS)
    {
      Sphere s(xyz[0], xyz[1], xyz[2], r);
      las->query(&s, pts, &lasfilter);
    }
    else
    {
      las->knn(xyz, k, r, pts, &lasfilter);
    }

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
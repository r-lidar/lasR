#include "nnmetrics.h"
#include "localmaximum.h"
#include "openmp.h"
#include "error.h"

#define PUREKNN 0
#define KNNRADIUS 1
#define PURERADIUS 2

bool LASRnnmetrics::set_parameters(const nlohmann::json& stage)
{
  k = stage.at("k");
  r = stage.at("r");
  std::vector<std::string> methods = ::get_vector<std::string>(stage["metrics"]);

  if (k == 0 && r > 0) mode = PURERADIUS;
  else if (k > 0 && r == 0) mode = PUREKNN;
  else if (k > 0 && r > 0) mode = KNNRADIUS;
  else
  {
    last_error = "Internal error: invalid argument k or r"; // # nocov
    return false;
  }

  if (mode == PUREKNN) this->r = F64_MAX;

  if (!metrics.parse(methods)) return false;

  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPoint25D);
  for (const auto& attr : methods) vector.add_field(attr, OFTReal);

  return true;
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
  progress->set_ncpu(ncpu);

  lm.resize(maxima.size());

  #pragma omp parallel for num_threads(ncpu) firstprivate(metrics)
  for (size_t i = 0 ; i < maxima.size() ; i++)
  {
    if (progress->interrupted()) continue;

    const PointLAS& p = maxima[i];

    std::vector<Point> pts;
    if (mode == PURERADIUS)
    {
      Sphere s(p.x, p.y, p.z, r);
      las->query(&s, pts, &pointfilter);
    }
    else
    {
      Point pt;
      pt.set_schema(&las->newheader->schema);
      pt.set_x(p.x);
      pt.set_y(p.y);
      pt.set_z(p.z);
      las->knn(pt, k, r, pts, &pointfilter);
    }

    PointXYZAttrs pt(p.x, p.y, p.z);
    pt.vals.reserve(metrics.size());
    for (int i = 0 ; i < metrics.size() ; i++)
    {
      float val = metrics.get_metric(i, pts);
      pt.vals.push_back(val);
    }

    lm[i] = pt;

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

bool LASRnnmetrics::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  LASRlocalmaximum* p = dynamic_cast<LASRlocalmaximum*>(s);

  if (p)
    set_connection(p);
  else
  {
    last_error = "Incompatible stage combination for 'neighborhood_metrics'"; // # nocov
    return false; // # nocov
  }

  return true;
}
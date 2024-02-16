#include "localmaximum.h"
#include "openmp.h"

#include <chrono>

LASRlocalmaximum::LASRlocalmaximum(double xmin, double ymin, double xmax, double ymax, double ws, double min_height, std::string use_attribute)
{
  counter = 0;
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->ofile = ofile;

  this->ws = ws;
  this->min_height = min_height;

  this->use_attribute = use_attribute;



  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPoint25D);
  vector.set_fields_for(Vector::writable::POINTLAS);
}

bool LASRlocalmaximum::process(LAS*& las)
{
  if (!las)
  {
    last_error = "Uninitialized pointer to LAS object"; // # nocov
    return false; // # nocov
  }

  LAStransform* lastransform = nullptr;
  if (use_attribute != "Z")
  {
    lastransform = las->make_z_transformer(use_attribute);
    if (lastransform == nullptr)
    {
      char buffer[64];
      snprintf(buffer, sizeof(buffer), "no extrabyte attribute '%s' found", use_attribute.c_str());
      last_error = std::string(buffer);
      return false;
    }
  }

  progress->set_total(las->npoints);
  progress->set_prefix("Local maximum");

  // Local maximum algorithm
  double hws = ws/2;
  std::vector<U8> status(las->npoints);
  std::fill(status.begin(), status.end(), UKN);

  auto start_time = std::chrono::high_resolution_clock::now();

  PointLAS pp;
  #pragma omp parallel for num_threads(ncpu) private(pp)
  for (auto i = 0 ; i < las->npoints ; ++i)
  {
    #pragma omp critical
    {
      (*progress)++;
    }
    if (omp_get_thread_num() == 0)
    {
      progress->show();
    }

    if (!las->get_point(i, pp, &lasfilter, lastransform)) { status[i] = NLM; continue; } // The point was either filtered or withhelded
    if (pp.z < min_height) { status[i] = NLM; continue; }
    if (status[i] == NLM) { continue; }

    status[i] = LMX;

    Circle windows(pp.x, pp.y, hws);
    std::vector<PointLAS> points;
    las->query(&windows, points, &lasfilter, lastransform);

    // It seems there is a data race here but no because in the worst case the update status[pt.FID]
    // is non-synchronize with other iterations and it will simply prevent skipping one computation early
    for (auto& pt : points)
    {
      if (pt.z == pp.z && pt.x!= pp.x && pt.y != pp.y && status[pt.FID] == LMX) status[i] = NLM; // Handle duplicated height for different points
      if (pt.z > pp.z) status[i] = NLM;  // If the point is above the central one, the central one is not a LM
      if (pt.z < pp.z) status[pt.FID] = NLM; // If the point is below the central we can pretag it as not a LM
    }

    #pragma omp critical
    {
      if (status[i] == LMX)
      {
        // If the point is in the buffer we must guarantee it will be assigned the same ID the next
        // time we meet it. FID is a 64 bit geographic ID that is guaranteed to be unique. But we need
        // a 32 bit ID so we have a correspondence table.
        /*if (pp.x < xmin || pp.y < ymin || pp.x > xmax || pp.y > ymax)
         {

         }*/

        uint64_t FID = ((uint64_t)las->point.quantizer->get_X(pp.x) << 32) | (uint64_t)(las->point.quantizer->get_Y(pp.y));
        auto it = unicity_table.find(FID);
        if (it == unicity_table.end())
        {
          unicity_table[FID] = counter;
          lm.push_back(pp);
          lm.back().FID = counter;
          counter++;
        }
        else
        {
          lm.push_back(pp);
          lm.back().FID = it->second;
        }
      }
    }
  }

  progress->done();

  if (lastransform) delete lastransform;

  if (verbose)
  {
    // # nocov start
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    float second = (float)duration.count()/1000.0f;
    print("  Local Maximum Filter took %.2f sec.\n", second);
    // # nocov end
  }

  return true;
}

bool LASRlocalmaximum::write()
{
  int dupfid= 0;
  progress->reset();
  progress->set_total(lm.size());
  progress->set_prefix("Write local maxima on disk");

  if (lm.size() == 0) print("0 point to write\n");

  for (const auto& p : lm)
  {
    if (!vector.write(p))
    {
      if (last_error_code != GDALdataset::DUPFID)
        return false;
      else
        dupfid++;
    }
    (*progress)++;
    progress->show();
  }

  if (dupfid)
    warning("%d points skipped: trying to insert points with and FID that is already in the database. This may be due to overlapping tiles.", dupfid);

  return true;
}

void LASRlocalmaximum::clear(bool last)
{
  lm.clear();
}
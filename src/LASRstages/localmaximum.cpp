#include "localmaximum.h"
#include "openmp.h"

#include <chrono>

LASRlocalmaximum::LASRlocalmaximum(double xmin, double ymin, double xmax, double ymax, double ws, double min_height, Stage* algorithm)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  this->ws = ws;
  this->min_height = min_height;

  this->use_raster = true;

  this->use_attribute = "Z";

  set_connection(algorithm);

  counter = std::make_shared<unsigned int>(0);
  unicity_table = std::make_shared<std::unordered_map<uint64_t, unsigned int>>();

  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPoint25D);
}

LASRlocalmaximum::LASRlocalmaximum(double xmin, double ymin, double xmax, double ymax, double ws, double min_height, std::string use_attribute, bool record_attributes)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;

  this->ws = ws;
  this->min_height = min_height;

  this->use_raster = false;

  this->use_attribute = use_attribute;

  counter = std::make_shared<unsigned int>(0);
  unicity_table = std::make_shared<std::unordered_map<uint64_t, unsigned int>>();

  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPoint25D);

  if (record_attributes)
    vector.set_fields_for(Vector::writable::POINTLAS);
}

bool LASRlocalmaximum::process()
{
  // Not working on a raster
  if (!use_raster) return true;

  auto it = connections.begin();
  StageRaster* p = dynamic_cast<StageRaster*>(it->second);
  const Raster& raster = p->get_raster();

  // Convert the raster to a LAS object to recycle the point cloud based local maximum
  LAS las(raster);
  LAS* ptr = &las;

  // Process the LAS
  use_raster = false; // deactivate to process a LAS
  bool success = process(ptr);
  use_raster = true;

  return success;
}

bool LASRlocalmaximum::process(LAS*& las)
{
  if (use_raster) return true;

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

  progress->reset();
  progress->set_total(las->npoints);
  progress->set_prefix("Local maximum");

  // Local maximum algorithm
  double hws = ws/2;
  std::vector<char> status(las->npoints);
  std::fill(status.begin(), status.end(), UKN);

  auto start_time = std::chrono::high_resolution_clock::now();

  // The next for loop is at the level 2 of a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  #pragma omp parallel for num_threads(ncpu)
  for (auto i = 0 ; i < las->npoints ; ++i)
  {
    if (progress->interrupted()) continue;

    PointLAS pp;

    if (main_thread)
    {
      #pragma omp critical
      {
        // can only be called in outer thread 0 AND is internally thread safe being called only in outer thread 0
        (*progress)++;
        progress->show();
      }
    }

    if (!las->get_point(i, pp, &lasfilter, lastransform)) { status[i] = NLM; } // The point was either filtered or withhelded
    if (pp.z < min_height) { status[i] = NLM; }
    if (status[i] == NLM) continue;

    Circle windows(pp.x, pp.y, hws);
    std::vector<PointLAS> points;
    las->query(&windows, points, &lasfilter, lastransform);

    // It seems there is a data race here but no. In the worst case updating status[pt.FID]
    // is non-synchronized with other iterations and it will simply prevent skipping one computation early
    for (auto& pt : points)
    {
      if (pt.z == pp.z && pt.x != pp.x && pt.y != pp.y && status[pt.FID] == LMX) status[i] = NLM; // Handle duplicated height for different points
      if (pt.z > pp.z) status[i] = NLM;  // If the point is above the central one, the central one is not a LM
      if (pt.z < pp.z) status[pt.FID] = NLM; // If the point is below the central we can pretag it as not a LM (no data race)
    }

    if (status[i] == UKN) status[i] = LMX; // If the status is still unknown it is a local max

    if (status[i] == LMX)
    {
      #pragma omp critical(assign_lm_ids)
      {
        // If the point is in the buffer we must guarantee it will be assigned the same ID the next
        // time we meet it. FID is a 64 bit geographic ID that is guaranteed to be unique. But we need
        // a 32 bit ID so we have a correspondence table.
        uint64_t FID = ((uint64_t)las->point.quantizer->get_X(pp.x) << 32) | (uint64_t)(las->point.quantizer->get_Y(pp.y));
        auto it = unicity_table->find(FID);
        if (it == unicity_table->end())
        {
          (*unicity_table)[FID] = *counter;
          lm.push_back(pp);
          lm.back().FID = *counter;
          (*counter)++;
        }
        else
        {
          lm.push_back(pp);
          lm.back().FID = it->second;
        }
      }
    }
  }

  if (lastransform) delete lastransform;

  progress->done();

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

  if (lm.size() == 0) return true;

  for (const auto& p : lm)
  {
    bool success;
    #pragma omp critical (write_localmax)
    {
      success = vector.write(p);
    }

    if (!success)
    {
      // /!\ TODO: not thread safe
      if (last_error_code != GDALdataset::DUPFID)
      {
        return false;
      }
      else
      {
        dupfid++;
        last_error_code = 0;
      }
    }

    (*progress)++;
    progress->show();
  }

  if (dupfid)
    print("%d points skipped with duplicated FID. This may be due to overlapping tiles or duplicated points.\n", dupfid);

  return true;
}

void LASRlocalmaximum::clear(bool last)
{
  lm.clear();
}
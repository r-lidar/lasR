#include "localmaximum.h"
#include "openmp.h"

#include <chrono>

LASRlocalmaximum::LASRlocalmaximum()
{
  this->use_raster = false;
  this->counter = std::make_shared<unsigned int>(0);
  this->unicity_table = std::make_shared<std::unordered_map<uint64_t, unsigned int>>();
}

bool LASRlocalmaximum::set_parameters(const nlohmann::json& stage)
{
  ws = stage.at("ws");
  min_height = stage.value("min_height", 2.0);

  use_attribute = stage.value("use_attribute", "Z");
  record_attributes = stage.value("record_attributes", false);

  vector = Vector(xmin, ymin, xmax, ymax);
  vector.set_geometry_type(wkbPoint25D);

  if (record_attributes)
  {
    vector.add_field("Intensity", OFTInteger);
    vector.add_field("gpstime", OFTReal);
    vector.add_field("ReturnNumber", OFTInteger);
    vector.add_field("Classification", OFTInteger);
    vector.add_field("ScanAngle", OFTReal);
  }

  if (connections.size() > 0) use_raster = true;

  return true;
}

bool LASRlocalmaximum::process()
{
  // Not working on a raster
  if (!use_raster) return true;

  auto it = connections.begin();
  StageRaster* p = dynamic_cast<StageRaster*>(it->second);
  const Raster& raster = p->get_raster();

  // Convert the raster to a LAS object to recycle the point cloud based local maximum
  PointCloud las(raster);
  PointCloud* ptr = &las;

  // Process the LAS
  use_raster = false; // deactivate to process a LAS
  bool success = process(ptr);
  use_raster = true;

  return success;
}

bool LASRlocalmaximum::process(PointCloud*& las)
{
  if (use_raster) return true;

  if (!las)
  {
    last_error = "Uninitialized pointer to LAS object"; // # nocov
    return false; // # nocov
  }

  AttributeAccessor accessor(use_attribute);
  AttributeAccessor get_intensity("Intensity");
  AttributeAccessor get_angle("Angle");
  AttributeAccessor get_return("ReturnNumber");
  AttributeAccessor get_number("NumberOfReturns");

  progress->reset();
  progress->set_total(las->npoints);
  progress->set_prefix("Local maximum");
  progress->set_ncpu(ncpu);

  // Local maximum algorithm
  double hws = ws/2;
  std::vector<char> status(las->npoints);
  std::fill(status.begin(), status.end(), UKN);

  auto start_time = std::chrono::high_resolution_clock::now();

  // The next for loop is at the level 2 of a nested parallel region. Printing the progress bar
  // is not thread safe. We first check that we are in outer thread 0
  bool main_thread = omp_get_thread_num() == 0;

  if (verbose) print("Building grid partition spatial index\n");
  las->build_partition();


  #pragma omp parallel for num_threads(ncpu)
  for (size_t i = 0 ; i < las->npoints ; ++i)
  {
    if (progress->interrupted()) continue;

    Point pp;
    pp.set_schema(&las->header->schema);

    if (main_thread)
    {
      #pragma omp critical
      {
        // can only be called in outer thread 0 AND is internally thread safe being called only in outer thread 0
        (*progress)++;
        progress->show();
      }
    }

    if (!las->get_point(i, &pp, &pointfilter)) { status[i] = NLM; } // The point was either filtered or withhelded
    if (accessor(&pp) < min_height) { status[i] = NLM; }
    if (status[i] == NLM) continue;

    Circle windows(pp.get_x(), pp.get_y(), hws);
    std::vector<Point> points;
    las->query(&windows, points, &pointfilter);

    // It seems there is a data race here but no. In the worst case updating status[pt.FID]
    // is non-synchronized with other iterations and it will simply prevent skipping one computation early
    for (auto& pt : points)
    {
      int fid = las->get_index(&pt);
      if (accessor(&pt) == accessor(&pp) && (pt.get_x() != pp.get_x() || pt.get_y() != pp.get_y()) && status[fid] == LMX) status[i] = NLM; // Handle duplicated height for different points
      if (accessor(&pt) > accessor(&pp)) status[i] = NLM;  // If the point is above the central one, the central one is not a LM
      if (accessor(&pt) < accessor(&pp)) status[fid] = NLM; // If the point is below the central we can pretag it as not a LM (no data race)
    }

    if (status[i] == UKN) status[i] = LMX; // If the status is still unknown it is a local max

    if (status[i] == LMX)
    {
      #pragma omp critical(assign_lm_ids)
      {
        // If the point is in the buffer we must guarantee it will be assigned the same ID the next
        // time we meet it. FID is a 64 bit geographic ID that is guaranteed to be unique. But we need
        // a 32 bit ID so we have a correspondence table.
        uint64_t FID = ((uint64_t)pp.get_X() << 32) | (uint64_t)(pp.get_Y());
        auto it = unicity_table->find(FID);

        PointLAS plas;
        plas.x = pp.get_x();
        plas.y = pp.get_y();
        plas.z = pp.get_z();
        plas.intensity = get_intensity(&pp);
        plas.return_number = get_return(&pp);
        plas.scan_angle = get_angle(&pp);
        plas.number_of_returns = get_number(&pp);
        if (it == unicity_table->end())
        {
          (*unicity_table)[FID] = *counter;
          lm.push_back(plas);
          lm.back().FID = *counter;
          (*counter)++;
        }
        else
        {
          lm.push_back(plas);
          lm.back().FID = it->second;
        }
      }
    }
  }

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
  if (ofile.empty()) return true;

  auto start_time = std::chrono::high_resolution_clock::now();

  if (lm.size() == 0) return true;

  bool success;
  #pragma omp critical (write_localmax)
  {
    success = vector.write(lm, record_attributes);
  }

  if (!success)
    return false;

  int dupfid = vector.get_dupfid();
  if (dupfid) print("%d points skipped with duplicated FID. This may be due to overlapping tiles or duplicated points.\n", dupfid);

  if (verbose)
  {
    // # nocov start
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    float second = (float)duration.count()/1000.0f;
    print("  Local Maximum write took %.2f sec.\n", second);
    // # nocov end
  }

  return true;
}

void LASRlocalmaximum::clear(bool last)
{
  lm.clear();
}

bool LASRlocalmaximum::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  StageRaster* p = dynamic_cast<StageRaster*>(s);
  if (p)
  {
    set_connection(p);
  }
  else
  {
    last_error = "Incompatible stage combination for local_maximum"; // # nocov
    return false; // # nocov
  }

  return true;
}
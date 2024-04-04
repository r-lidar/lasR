#include "pipeline.h"
#include "LAS.h"
#include "LAScatalog.h"
#include "Stage.h"
#include "Progress.h"
#include "macros.h"
#include "openmp.h"

#include <algorithm>

Pipeline::Pipeline()
{
  ncpu = 1;
  parsed = false;
  verbose = false;
  streamable = true;
  read_payload = false;
  buffer = 0;

  header = nullptr;
  point = nullptr;
  las = nullptr;
  catalog = nullptr;
}

// The copy constructor is used for multi-threading. It creates a copy of the pipeline
// where each stage is deep copied but with shared resources  such as file connection and
// gdal datasets
Pipeline::Pipeline(const Pipeline& other)
{
  ncpu = other.ncpu;
  parsed = other.parsed;
  verbose = other.verbose;
  streamable = other.streamable;
  read_payload = other.read_payload;
  buffer = other.buffer;

  header = nullptr;
  point = nullptr;
  las = nullptr;

  catalog = other.catalog;

  for (const auto& stage : other.pipeline)
  {
    Stage* ptr = stage->clone();
    pipeline.push_back(std::unique_ptr<Stage>(ptr));
  }

  // At this stage, we have a clone of the pipeline. However some stages may contain a pointer
  // to another stage they are connected to. The pointers have been copied 'has is' because
  // we don't the new address of the stages. We need to update the connections to other stages
  // with the new addresses.
  // We can loop over all the stages, and for each stage that come after this one, update the
  // connections with this stage. The matching is done by the UID of each stage so if the two
  // stage are not connected nothing happens.
  for (auto it1 = pipeline.begin(); it1 != pipeline.end(); ++it1)
  {
    for (auto it2 = it1; it2 != pipeline.end(); ++it2)
    {
      (*it2)->update_connection(it1->get());
    }
  }
}

Pipeline::~Pipeline()
{
  clean();
}

bool Pipeline::pre_run()
{
  LAScatalog* ctg = catalog.get();
  for (auto&& stage : pipeline)
  {
    bool success = stage->process(ctg);
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while processing the catalog: " + last_error; // # nocov
      return false; // # nocov
    }
  }

  return true;
}

bool Pipeline::run()
{
  bool success;

  if (streamable)
    success = run_streamed();
  else
    success = run_loaded();

  clear();
  clean();

  return success;
}

bool Pipeline::run_streamed()
{
  bool success;
  bool last_point = false;

  while(!last_point)
  {
    for (auto&& stage : pipeline)
    {
      // Some stage process the header. The first stage being a reader, the LASheader, which is
      // initially nullptr, will be initialized by pipeline[0]
      success = stage->process(header);
      if (!success)
      {
        last_error = "in '" + stage->get_name() + "' while processing the header: " + last_error;
        return false;
      }

      // Special case: pipeline[0] could be write_lax, in this case the first stage does not
      // initialize the header. We must go to pipeline[1] immediately
      if (header == nullptr) continue;

      // There is no point to read
      uint64_t npoints = 0;
      npoints += header->number_of_point_records;
      npoints += header->extended_number_of_point_records;
      if (npoints == 0) return true;

      // Some stages need the header to get initialized (write_las is the only one)
      stage->set_header(header);

      // Each stage process the LASpoint. The first stage being a reader, the LASpoint, which is
      // initially nullptr, will be initialized by pipeline[0]
      if (read_payload)
      {
        success = stage->process(point);

        if (!success)
        {
          last_error = "in '" + stage->get_name() + "' while processing a point: " + last_error; // # nocov
          return false; // # nocov
        }

        if (point == nullptr)
        {
          last_point = true;
          break;
        }
      }
      else
      {
        last_point = true;
      }
    }
  }

  // Each stage is writing its own output
  for (auto&& stage : pipeline)
  {
    success = stage->write();
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while writing the output: " + last_error;
      return false;
    }
  }

  return true;
}

bool Pipeline::run_loaded()
{
  bool success;

  for (auto&& stage : pipeline)
  {
    if (verbose) print("Stage: %s\n", stage->get_name().c_str());

    // Some stages need no input, they are connected to another stage
    // (such as pit_fill which is connected to a raster stage and does not need any point)
    success = stage->process();
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while processing: " + last_error;
      return false;
    }

    // Some stages process the header. The first stage being a reader, the LASheader, which is
    // initially nullptr, will be initialized by pipeline[0]
    success = stage->process(header);
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while processing the header: " + last_error;
      return false;
    }

    // Special case: pipeline[0] could be write_lax, in this case the first stage does not
    // initialize the header. We must go to pipeline[1] immediately
    if (header == nullptr) continue;

    // There is no point to read
    uint64_t npoints = 0;
    npoints += header->number_of_point_records;
    npoints += header->extended_number_of_point_records;
    if (npoints == 0) return true;

    // Some stages need the header to get initialized (write_las is the only one)
    stage->set_header(header);

    // The pipeline is not streamable we need an object that stores and spatially index all the points
    // Each stage process the LAS. The first stage being a reader, the LAS, which is initially nullptr,
    // will be initialized by pipeline[0] (or pipeline[1] if there is a write_lax stage)
    if (read_payload)
    {
      success = stage->process(las);
      if (!success)
      {
        last_error = "in '" + stage->get_name() + "' while processing the point cloud: " + last_error;
        return false;
      }
    }

    // Each stage is writing its own output
    success = stage->write();
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while writing the output: " + last_error;
      return false;
    }
  }

  return true;
}

void Pipeline::merge(const Pipeline& other)
{
  order.insert(order.end(), other.order.begin(), other.order.end());

  auto it1 = this->pipeline.begin();
  auto it2 = other.pipeline.begin();

  while (it1 != this->pipeline.end() && it2 != other.pipeline.end())
  {
    (*it1)->merge(it2->get());
    ++it1;
    ++it2;
  }
}

bool Pipeline::set_chunk(const Chunk& chunk)
{
  order.push_back(chunk.id);

  for (auto&& stage : pipeline)
  {
    if (!stage->set_chunk(chunk))
    {
      last_error = "in " + stage->get_name() + " while initalizing: " + last_error; // # nocov
      return false; // # nocov
    }

    stage->set_input_file_name(chunk.name);
  }

  return true;
}

void Pipeline::set_progress(Progress* progress)
{
  for (auto&& stage : pipeline) stage->set_progress(progress);
}

void Pipeline::set_ncpu(int ncpu)
{
  if (ncpu > available_threads())
  {
    warning("Number of cores requested %d but only %d available", ncpu, available_threads()); // # nocov
    ncpu = available_threads(); // # nocov
  }

  this->ncpu = ncpu;
  for (auto&& stage : pipeline) stage->set_ncpu(ncpu);
}

bool Pipeline::set_crs(int epsg)
{
  if (epsg == 0) return false;
  for (auto&& stage : pipeline) stage->set_crs(epsg);
  return true;
}

bool Pipeline::set_crs(std::string wkt)
{
  if (wkt.empty()) return false;
  for (auto&& stage : pipeline) stage->set_crs(wkt);
  return true;
}

void Pipeline::set_verbose(bool verbose)
{
  this->verbose = verbose;
  for (auto&& stage : pipeline) stage->set_verbose(verbose);
}

bool Pipeline::is_streamable() const
{
  bool b = true;
  for (auto&& stage : pipeline)
  {
    if (!stage->is_streamable())
    {
      b = false;
      break;
    }
  }

  return b;
}

bool Pipeline::is_parallelizable() const
{
  bool b = true;
  for (auto&& stage : pipeline)
  {
    if (!stage->is_parallelizable())
    {
      b = false;
      break;
    }
  }

  return b;
}

bool Pipeline::is_parallelized() const
{
  bool b = false;
  for (auto&& stage : pipeline)
  {
    if (stage->is_parallelized())
    {
      b = true;
      break;
    }
  }

  return b;
}

bool Pipeline::use_rcapi() const
{
  bool b = false;
  for (auto&& stage : pipeline)
  {
    if (stage->use_rcapi())
    {
      b = true;
      break;
    }
  }

  return b;
}

bool Pipeline::need_points() const
{
  bool b = false;
  for (auto&& stage : pipeline)
  {
    if (stage->need_points())
    {
      b = true;
      break;
    }
  }

  return b;
}

double Pipeline::need_buffer()
{
  for (auto&& stage : pipeline)
  {
    if (!stage->is_streamable())
    {
      buffer = MAX(buffer, stage->need_buffer());
    }
  }

  return buffer;
}

void Pipeline::sort()
{
  // The vector 'order' contain the chunk IDs in the order they were computed. In sequential processing
  // this order is trivially 1 2 3 4 5 ... but in parallel the order is unpredictable like 1 5 3 2 4.
  // sorting reorder the results such as parallel processing is order preserving. BUT !! Some chunk may
  // be skipped if some file are flagged as 'noprocess' (buffer) only. In this case the order is
  // 1 5 2 4 with the last index being 5 but only for 4 output. This segfault see #22. Consequently we
  // recompute the order ensuring to have valid indexes before to sort
  std::vector<std::pair<int, int>> index;
  for (int i = 0; i < order.size(); i++) index.push_back(std::make_pair(order[i], i));
  std::sort(index.begin(), index.end());
  for (int i = 0; i < order.size(); i++) order[i] = index[i].second;

  for (auto&& stage : pipeline)
  {
    stage->sort(order);
  }
}

void Pipeline::clear(bool last)
{
  for (auto&& stage : pipeline)
  {
    stage->clear(last);
  }
}

void Pipeline::clean()
{
  delete las;
  header = nullptr;
  point = nullptr;
  las = nullptr;
}

#ifdef USING_R
SEXP Pipeline::to_R()
{
  SEXP ans = PROTECT(Rf_allocVector(VECSXP, pipeline.size()));
  SEXP names = PROTECT(Rf_allocVector(STRSXP, pipeline.size()));
  int i = 0;
  for (auto&& stage : pipeline)
  {
    SET_STRING_ELT(names, i, Rf_mkChar(stage->get_name().c_str()));
    SET_VECTOR_ELT(ans, i, stage->to_R());
    i++;
  }
  Rf_setAttrib(ans, R_NamesSymbol, names);
  UNPROTECT(2);

  return ans;
}
#endif

/*void Pipeline::show()
{
  // # nocov start
  #pragma
  print("-------------\n");
  for (auto&& stage : pipeline)
  {
    print("Stage %s: %s at %p\n", stage->get_uid().c_str(), stage->get_name().c_str(), stage.get());
    for (auto con : stage->connections)
    {
      print("  connected to %s at %p\n", con.first.c_str(), con.second);
    }
  }
  print("-------------\n");
  // # nocov end
}*/
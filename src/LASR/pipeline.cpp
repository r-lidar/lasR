#include "pipeline.h"
#include "LAS.h"
#include "LAScatalog.h"
#include "lasralgorithm.h"
#include "Progress.h"
#include "macros.h"
#include "openmp.h"

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
    LASRalgorithm* ptr = stage->clone();
    if (ptr == nullptr)
    {
      last_error = "Internal error: the stage '" + stage->get_name() + "' does not have valid clone() method";
      throw last_error;
    }
    pipeline.push_back(std::unique_ptr<LASRalgorithm>(ptr));
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
  // Only used by write_vpc
  LAScatalog* ctg = catalog.get();
  return process(ctg);
}

bool Pipeline::run()
{
  bool success;

  // Each stage process the LASheader. The first stage being a reader, the LASheader, which is
  // initially nullptr, will be initialized by pipeline[0]
  success = process(header);
  if (!success)  return false;

  if (header->number_of_point_records == 0 && header->extended_number_of_point_records == 0)
  {
    return true; // # nocov
  }

  set_header(header);

  // If the pipeline is streamable can process all the point without storing them
  // Each stage process the LASpoint. The first stage being a reader, the LASpoint, which is
  // initially nullptr, will be initialized by pipeline[0]
  success = process(point);
  if (!success) { return false; }

  // If the pipeline is not streamable we need an object that stores and spatially index all the point
  // Each stage process the LAS. The first stage being a reader, the LAS, which is
  // initially nullptr, will be initialized by pipeline[0]
  success = process(las);
  if (!success)
  {
    clean();
    return false;
  }

  success = write();
  if (!success)
  {
    clean();
    return false;
  }

  clean();
  return true;
}

void Pipeline::merge(const Pipeline& other)
{
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
    warning("Number of cores requested %d but only %d available", ncpu, available_threads());
    ncpu = available_threads();
  }
  this->ncpu = ncpu;
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

void Pipeline::set_header(LASheader*& header)
{
  for (auto&& stage : pipeline) stage->set_header(header);
}

void Pipeline::set_verbose(bool verbose)
{
  this->verbose = verbose;
}

bool Pipeline::process(LASheader*& header)
{
  bool success;
  for (auto&& stage : pipeline)
  {
    success = stage->process(header);
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while processing the header: " + last_error; // # nocov
      return false; // # nocov
    }
  }

  return true;
}

bool Pipeline::process(LASpoint*& point)
{
  if (!read_payload || !streamable)
    return true;

  bool last_point = false;
  while(!last_point)
  {
    for (auto&& stage : pipeline)
    {
      bool success = stage->process(point);

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
  }

  return true;
}

bool Pipeline::process(LAS*& las)
{
  if (!read_payload || streamable)
    return true;

  for (auto&& stage : pipeline)
  {
    if(verbose) print(" %s in thread %d\n", stage->get_name().c_str(), omp_get_thread_num());

    bool success = stage->process(las);
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while processing the point cloud: " + last_error;
      return false;
    }
  }

    return true;
}

bool Pipeline::process(LAScatalog*& catalog)
{
  for (auto&& stage : pipeline)
  {
    bool success = stage->process(catalog);
    if (!success)
    {
      last_error = "in '" + stage->get_name() + "' while processing the catalog: " + last_error; // # nocov
      return false; // # nocov
    }
  }

  return true;
}

bool Pipeline::write()
{
  for (auto&& stage : pipeline)
  {
    bool success;
    success = stage->write();
    if (!success)
    {
      last_error = "in " + stage->get_name() + " while writing the output: " + last_error;
      return false;
    }
  }

  return true;
}

bool Pipeline::is_streamable()
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

bool Pipeline::is_parallelizable()
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

bool Pipeline::need_points()
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

void Pipeline::clear(bool last)
{
  for (auto&& stage : pipeline)
  {
    stage->clear(last);
  }
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

void Pipeline::clean()
{
  delete las;
  header = nullptr;
  point = nullptr;
  las = nullptr;
}

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
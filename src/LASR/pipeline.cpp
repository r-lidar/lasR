#include "pipeline.h"
#include "LAS.h"
#include "LAScatalog.h"
#include "lasralgorithm.h"
#include "Progress.h"
#include "macros.h"
#include "openmp.h" // available_threads()

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

// The copy constructor is used for multithreading. It creates a copy of pipeline
// where each stage is deep copyed but with shared ressource  such as file connection and
// gdal datasets.
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
    pipeline.push_back(std::unique_ptr<LASRalgorithm>(stage->clone()));
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
  if (!success) return false;

  if (header->number_of_point_records == 0 && header->extended_number_of_point_records == 0)
  {
    return true; // # nocov
  }

  set_header(header);

  // If the pipeline is streamable can process all the point without storing them
  // Each stage process the LASpoint. The first stage being a reader, the LASpoint, which is
  // initially nullptr, will be initialized by pipeline[0]
  success = process(point);
  if (!success) return false;

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
  print("Pipeline::merge\n");

  auto it1 = this->pipeline.begin();
  auto it2 = other.pipeline.begin();

  while (it1 != this->pipeline.end() && it2 != other.pipeline.end())
  {
    print("stage: %s / %s\n", (*it1)->get_name().c_str(), (*it2)->get_name().c_str());
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

/*void Pipeline::set_buffer(double buffer)
{
  this->buffer = buffer;
}*/

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
    if(verbose) print(" %s\n", stage->get_name().c_str());

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
    bool success = stage->write();
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
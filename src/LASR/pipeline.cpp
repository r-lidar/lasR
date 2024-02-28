#include "pipeline.h"
#include "LAS.h"
#include "LAScatalog.h"
#include "lasralgorithm.h"
#include "Progress.hpp"
#include "macros.h"
#include "openmp.h" // available_threads()

Pipeline::Pipeline()
{
  read_payload = false;
  streamable = true;
  buffer = 0;
  parsed = false;
  verbose = false;
  header = nullptr;
  point = nullptr;
  las = nullptr;
  catalog = nullptr;
}

Pipeline::~Pipeline()
{
  clean();
  delete catalog;
  catalog = nullptr;
}

bool Pipeline::pre_run()
{
  // Only used by write_vpc
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

bool Pipeline::run()
{
  bool success;

  if (streamable)
    success = run_streamed();
  else
    success = run_loaded();

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
      if (!success) return false;

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
    if (!success) return false;
  }

  return true;
}

bool Pipeline::run_loaded()
{
  bool success;

  for (auto&& stage : pipeline)
  {
    // Some stage process the header. The first stage being a reader, the LASheader, which is
    // initially nullptr, will be initialized by pipeline[0]
    success = stage->process(header);
    if (!success) return false;

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

    // If the pipeline is not streamable we need an object that stores and spatially index all the point
    // Each stage process the LAS. The first stage being a reader, the LAS, which is
    // initially nullptr, will be initialized by pipeline[0]
    if (read_payload)
    {
      success = stage->process(las);
      if (!success) return false;
    }

    // Each stage is writing its own output
    success = stage->write();
    if (!success) return false;
  }

  return true;
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

void Pipeline::set_verbose(bool verbose)
{
  this->verbose = verbose;
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
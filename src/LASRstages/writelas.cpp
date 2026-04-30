#include "writelas.h"

#include "LASio.h"

#include <cmath>      // std::isfinite
#include <exception>  // std::exception_ptr, std::current_exception

LASRlaswriter::LASRlaswriter()
{
  lasio = nullptr;
  copc_bbox_set = false;
  copc_bbox_xmin = copc_bbox_ymin = copc_bbox_zmin = 0.0;
  copc_bbox_xmax = copc_bbox_ymax = copc_bbox_zmax = 0.0;
}

LASRlaswriter::~LASRlaswriter() noexcept
{
  // Destructors must not throw. lasio->close() can now throw a runtime_error
  // when the COPC writer's finalize fails; if we let that escape during
  // stack unwinding from another exception, std::terminate gets called.
  // Swallow with a warning — the close path also unlinks the partial output
  // file on its end so the user doesn't end up with a corrupt file on disk.
  if (lasio)
  {
    // # nocov start
    warning("internal error: please report, a LASwriter is still opened when destructing LASRlaswriter. The LAS or LAZ file written may be corrupted\n");
    try
    {
      lasio->close();
    }
    catch (const std::exception& e)
    {
      warning("error during writer close in destructor (suppressed): %s\n", e.what());
    }
    catch (...)
    {
      warning("unknown error during writer close in destructor (suppressed)\n");
    }
    delete lasio;
    lasio = nullptr;
    // # nocov end
  }
}

bool LASRlaswriter::set_parameters(const nlohmann::json& stage)
{
  keep_buffer = stage.value("keep_buffer", false);
  experimental_writer = stage.value("experimental_writer", false);
  copc_density = stage.value("density", 256);
  copc_depth = stage.value("max_depth", -1);
  // JSON-default fallback if the key is missing matches the surface defaults
  // (R/Python/api.h). Hand-written JSON configs without this key get the
  // same single-level adaptive bumping that the language wrappers do.
  copc_max_extra_depth = stage.value("max_extra_depth", 1);
  copc_max_points_per_chunk = stage.value("max_points_per_chunk", -1);
  // Optional caller-provided bbox override for the COPC writer. JSON
  // shape: bbox = [xmin, ymin, zmin, xmax, ymax, zmax]. Only honoured
  // when experimental_writer = TRUE and the output is .copc.laz.
  // Validates that all 6 values are present and the box is non-degenerate.
  if (stage.contains("bbox") && stage["bbox"].is_array() && stage["bbox"].size() == 6)
  {
    const auto& b = stage["bbox"];
    copc_bbox_xmin = b[0].get<double>();
    copc_bbox_ymin = b[1].get<double>();
    copc_bbox_zmin = b[2].get<double>();
    copc_bbox_xmax = b[3].get<double>();
    copc_bbox_ymax = b[4].get<double>();
    copc_bbox_zmax = b[5].get<double>();
    // Reject non-finite values (NaN/Inf): the std::isless / > comparisons
    // below would not reliably reject them, and they'd propagate into
    // the octree geometry as garbage.
    if (!std::isfinite(copc_bbox_xmin) || !std::isfinite(copc_bbox_xmax) ||
        !std::isfinite(copc_bbox_ymin) || !std::isfinite(copc_bbox_ymax) ||
        !std::isfinite(copc_bbox_zmin) || !std::isfinite(copc_bbox_zmax))
    {
      last_error = "write_copc bbox: all 6 values must be finite (no NaN/Inf)";
      return false;
    }
    if (copc_bbox_xmin > copc_bbox_xmax ||
        copc_bbox_ymin > copc_bbox_ymax ||
        copc_bbox_zmin > copc_bbox_zmax)
    {
      last_error = "write_copc bbox: each min must be <= the corresponding max";
      return false;
    }
    copc_bbox_set = true;
  }
  version_minor = stage.value("version", 0xFF); // 0xFF auto-detect
  point_format = stage.value("pdrf", 0xFF);

  if (version_minor != 0xFF && version_minor > 4)
  {
    last_error = "Invalid version for LAS format";
    return false;
  }

  if (point_format != 0xFF && point_format > 10)
  {
    last_error = "Invalid point data format for LAS format";
    return false;
  }

  return true;
}


bool LASRlaswriter::set_input_file_name(const std::string& file)
{
  ofile = template_filename;
  ifile = file;
  size_t pos = ofile.find('*');

  if (pos != std::string::npos)
  {
    ofile.replace(pos, 1, ifile);
  }

  return true;
}

bool LASRlaswriter::set_output_file(const std::string& file)
{
  template_filename = file;
  clean_copc_ext(template_filename);

  size_t pos = file.find('*');
  if (pos == std::string::npos)
  {
    merged = true;
  }

  return true;
}

bool LASRlaswriter::process(Point*& p)
{
  // In streaming mode the point is owned by reader_las. Desallocating it stops the pipeline
  if (p == nullptr) return true;
  if (p->get_deleted()) return true;

  // No writer initialized? Create a writer.
  if (!lasio->is_opened())
  {
    try
    {
      lasio->create(ofile);
      written.push_back(ofile);
    }
    catch (const std::exception& e)
    {
      last_error = e.what();
      return false;
    }
  }

  //  If the point in not in the buffer we can write it
  if (keep_buffer || (!p->get_buffered() && !p->inside_buffer(xmin, ymin, xmax, ymax, circular)))
  {
    if (!pointfilter.filter(p))
    {
      // If we write in a merged file the points may come from different file formats
      /*if (merged)
       {
       if (p->quantizer->x_offset != offsets[0] || p->quantizer->x_scale_factor != scales[0])
       {
       double coordinate = (p->get_x() - offsets[0])/scales[0];
       p->set_X(I32_QUANTIZE(coordinate));
       }

       if (p->quantizer->y_offset != offsets[1] || p->quantizer->y_scale_factor != scales[1])
       {
       double coordinate = (p->get_y() - offsets[1])/scales[1];
       p->set_Y(I32_QUANTIZE(coordinate));
       }

       if (p->quantizer->z_offset != offsets[2] || p->quantizer->z_scale_factor != scales[2])
       {
       double coordinate = (p->get_z() - offsets[2])/scales[2];
       p->set_Z(I32_QUANTIZE(coordinate));
       }
       }*/

      // lasio->write_point can throw — the COPC writer reports per-point
      // I/O failures as runtime_error. Catch and report via last_error so
      // the engine sees a normal stage failure instead of an uncaught
      // exception (the parallel executor's catch in execute.cpp only
      // handles std::string, so an exception here would terminate the
      // worker thread).
      try
      {
        lasio->write_point(p);
      }
      catch (const std::exception& e)
      {
        last_error = std::string("error writing point: ") + e.what();
        return false;
      }
    }
  }

  return true;
}

bool LASRlaswriter::process(PointCloud*& las)
{
  progress->reset();
  progress->set_prefix("Write LAS");
  progress->set_total(las->npoints);

  Point* p;
  while (las->read_point())
  {
    p = &las->point;
    if (!process(p))
      return false; // # nocov

    (*progress)++;
    progress->show();
    if (progress->interrupted()) break;
  }

  progress->done();
  return true;
}

bool LASRlaswriter::set_header(Header*& header)
{
  // We are receiving a new header because a reader start reading a new file

  // If we still have an interface this means that we are merging multiple files
  // We don't need to create a new writer.
  if (lasio)
  {
    lasio->reset_accessor();
    return true;
  }

  // Use a tmp copy of the header to force some option without modifying the original header
  Header h = *header;
  h.signature = "LASF";
  h.point_data_format = point_format;
  h.version_minor = version_minor;

  try
  {
    lasio = new LASio();
    lasio->init(&h);
    lasio->set_copc_max_depth(copc_depth);
    lasio->set_copc_density(copc_density);
    lasio->set_use_new_copc_writer(experimental_writer);
    lasio->set_copc_max_extra_depth(copc_max_extra_depth);
    lasio->set_copc_max_points_per_chunk(copc_max_points_per_chunk);
    if (copc_bbox_set)
    {
      lasio->set_bbox_override(copc_bbox_xmin, copc_bbox_ymin, copc_bbox_zmin,
                               copc_bbox_xmax, copc_bbox_ymax, copc_bbox_zmax);
    }
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  return true;
}

bool LASRlaswriter::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  if (chunk.buffer == 0) keep_buffer = true;
  circular = chunk.shape == ShapeType::CIRCLE;
  return true;
}

void LASRlaswriter::clear(bool last)
{
  // In clear we are testing:
  // - is it the last call to clear? If yes we can clear everything
  // - are we writing in merge mode? If yes we need to keep the LASwriter. Otherwise we can clean
  //   a new writer will be created at next iteration
  if (!merged || last)
  {
    if (lasio)
    {
      // lasio->close can throw on COPC finalize failure. We must:
      //   (a) record the error,
      //   (b) clean up the lasio pointer regardless,
      //   (c) rethrow so the engine sees the failure.
      // Without (c) the failure is swallowed: Engine::run discards
      // clear()'s outcome and the parallel executor doesn't check
      // the cleanup either, so a corrupt-then-deleted .copc.laz would
      // be reported to the user as a successful write. We also keep
      // (a) and (b) to avoid leaking the writer if close throws.
      std::exception_ptr ex_ptr;
      try
      {
        lasio->close();
      }
      catch (const std::exception& e)
      {
        last_error = std::string("error closing writer: ") + e.what();
        ex_ptr = std::current_exception();
      }
      delete lasio;
      lasio = nullptr;
      if (ex_ptr) std::rethrow_exception(ex_ptr);
    }
  }
}

void LASRlaswriter::clean_copc_ext(std::string& path)
{
  const std::string suffix = ".copc.las";
  const std::string toRemove = ".copc";

  // Check if the path ends with .copc.las
  if (path.size() >= suffix.size() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
  {
    path.erase(path.size() - suffix.size(), toRemove.size()); // Remove .copc
  }
}

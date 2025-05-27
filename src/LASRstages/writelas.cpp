#include "writelas.h"

#include "LASio.h"

LASRlaswriter::LASRlaswriter()
{
  lasio = nullptr;
}

LASRlaswriter::~LASRlaswriter()
{
  if (lasio)
  {
    // # nocov start
    warning("internal error: please report, a LASwriter is still opened when destructing LASRlaswriter. The LAS or LAZ file written may be corrupted\n");
    lasio->close();
    delete lasio;
    lasio = nullptr;
    // # nocov end
  }
}

bool LASRlaswriter::set_parameters(const nlohmann::json& stage)
{
  keep_buffer = stage.value("keep_buffer", false);
  copc_density = stage.value("density", 256);
  copc_depth = stage.value("max_depth", -1);
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
    if (!lasio->create(ofile)) return false;
    written.push_back(ofile);
  }

  //  If the point in not in the buffer we can write it
  if (keep_buffer || !p->inside_buffer(xmin, ymin, xmax, ymax, circular))
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

      lasio->write_point(p);
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

void LASRlaswriter::set_header(Header*& header)
{
  // We are receiving a new header because a reader start reading a new file

  // If we still have an interface this means that we are merging multiple files
  // We don't need to create a new writer.
  if (lasio)
  {
    lasio->reset_accessor();
    return;
  }

  lasio = new LASio(progress);
  lasio->init(header);
  lasio->set_copc_max_depth(copc_depth);
  lasio->set_copc_density(copc_density);
}

bool LASRlaswriter::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  if (chunk.buffer == 0) keep_buffer = true;
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
      lasio->close();
      delete lasio;
      lasio = nullptr;
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

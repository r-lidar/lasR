#include "writepcd.h"

#include "PCDio.h"

LASRpcdwriter::LASRpcdwriter()
{
  pcdio = nullptr;
  binary = true;
}

LASRpcdwriter::~LASRpcdwriter()
{
  if (pcdio)
  {
    // # nocov start
    warning("internal error: please report, a pcdwriter is still opened when destructing LASRpcdwriter. The PCD file written may be corrupted\n");
    pcdio->close();
    delete pcdio;
    pcdio = nullptr;
    // # nocov end
  }
}

bool LASRpcdwriter::set_parameters(const nlohmann::json& stage)
{
  binary = stage.value("binary", true);
  return true;
}


bool LASRpcdwriter::set_input_file_name(const std::string& file)
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

bool LASRpcdwriter::set_output_file(const std::string& file)
{
  template_filename = file;

  size_t pos = file.find('*');
  if (pos == std::string::npos)
  {
    merged = true;
  }

  return true;
}

bool LASRpcdwriter::process(Point*& p)
{
  // In streaming mode the point is owned by reader_las. Desallocating it stops the pipeline
  if (p == nullptr) return true;
  if (p->get_deleted()) return true;

  // No writer initialized? Create a writer.
  if (!pcdio->is_opened())
  {
    try
    {
      pcdio->create(ofile);
      written.push_back(ofile);
    }
    catch (const std::exception& e)
    {
      last_error = e.what();
      return false;
    }
  }

  pcdio->write_point(p);

  return true;
}

bool LASRpcdwriter::process(PointCloud*& las)
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

bool LASRpcdwriter::set_header(Header*& header)
{
  // We are receiving a new header because a reader start reading a new file

  // If we still have an interface this means that we are merging multiple files
  // We don't need to create a new writer.
  if (pcdio)
  {
    pcdio->reset_accessor();
    return true;
  }

  try
  {
    pcdio = new PCDio();
    pcdio->init(header);
    pcdio->set_binary_mode(binary);
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  return true;
}

bool LASRpcdwriter::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk.xmin, chunk.ymin, chunk.xmax, chunk.ymax);
  if (chunk.buffer != 0)
  {
    last_error= "Buffered file not supported by PCD writer yet";
    return false;
  }
  return true;
}

void LASRpcdwriter::clear(bool last)
{
  // In clear we are testing:
  // - is it the last call to clear? If yes we can clear everything
  // - are we writing in merge mode? If yes we need to keep the pcdwriter. Otherwise we can clean
  //   a new writer will be created at next iteration
  if (!merged || last)
  {
    if (pcdio)
    {
      pcdio->close();
      delete pcdio;
      pcdio = nullptr;
    }
  }
}

#include "readpcd.h"

#include "PCDio.h"

LASRpcdreader::LASRpcdreader()
{
  header = nullptr;
  pcdio = nullptr;
  streaming = true;
}

bool LASRpcdreader::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  // New chunk -> new reader for a new file. We can delete the previous reader and build a new one
  if (pcdio)
  {
    pcdio->close();
    delete pcdio;
    pcdio = nullptr;
  }

  pcdio = new PCDio();

  try
  {
    pcdio->query(
        chunk.main_files,
        chunk.neighbour_files,
        chunk.xmin,
        chunk.ymin,
        chunk.xmax,
        chunk.ymax,
        chunk.buffer,
        chunk.shape == ShapeType::CIRCLE,
        filters);
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  return true;
}

bool LASRpcdreader::process(Header*& header)
{
  // LASRpcdreader is responsible for populating the header.
  // It is called first before LASRpcdreader::process(Point) (streaming) or LASRpcdreader::process(LAS) (in memory)
  // If the point is null then we create one Header. This object own the Header
  if (header != nullptr) return true;

  header = new Header;

  try
  {
    pcdio->populate_header(header);
  }
  catch (const std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  this->header = header;

  return true;
}

// Streaming mode
bool LASRpcdreader::process(Point*& point)
{
  if (point == nullptr)
    point = new Point(&header->schema);

  do
  {
    if (pcdio->read_point(point))
    {
      if (point->inside_buffer(xmin, ymin, ymax, ymax, circular))
        point->set_buffered();
    }
    else
    {
      // In streaming mode this triggers a stop
      delete point;
      point = nullptr;
    }
  } while (point != nullptr && pointfilter.filter(point));

  return true;
}

// In memory mode
bool LASRpcdreader::process(PointCloud*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new PointCloud(header);

  streaming = false;

  progress->reset();
  progress->set_total(header->number_of_point_records);
  progress->set_prefix("read_las");

  Point p(&header->schema);

  while (pcdio->read_point(&p))
  {
    if (progress->interrupted()) break;

    if (pointfilter.filter(&p)) continue;

    if (p.inside_buffer(xmin, ymin, xmax, ymax, circular))
      p.set_buffered();

    if (!las->add_point(p)) return false;

    progress->update(pcdio->p_count());
    progress->show();
  }

  las->update_header();

  progress->done();
  if (verbose) print(" Number of point read %d\n", las->npoints);

  if (verbose) print("Building a spatial index\n");
  las->update_header();

  return true;
}

LASRpcdreader::~LASRpcdreader()
{
  if (pcdio)
  {
    pcdio->close();
    delete pcdio;
    pcdio = nullptr;
  }
}

void LASRpcdreader::clear(bool)
{
  // Called at the end of the pipeline. We can delete the header
  if (streaming && header)
  {
    delete header;
    header = nullptr;
  }
}
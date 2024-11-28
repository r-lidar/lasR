#include "readlas.h"

#include "LASlibinterface.h"

LASRlasreader::LASRlasreader()
{
  header = nullptr;
  laslibinterface = nullptr;
  streaming = true;
}

bool LASRlasreader::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  // New chunk -> new reader for a new file. We can delete the previous reader and build a new one
  if (laslibinterface)
  {
    laslibinterface->close();
    delete laslibinterface;
    laslibinterface = nullptr;
  }

  laslibinterface = new LASlibInterface(progress);
  return laslibinterface->open(chunk, filters);
}

bool LASRlasreader::process(Header*& header)
{
  // LASRlasreader is responsible for populating the header.
  // It is called first before LASRlasreader::process(Point) (streaming) or LASRlasreader::process(LAS) (in memory)
  // If the point is null then we create one Header. This object own the Header
  if (header != nullptr) return true;

  header = new Header;
  laslibinterface->populate_header(header);

  this->header = header;

  return true;
}

// Streaming mode
bool LASRlasreader::process(Point*& point)
{
  if (point == nullptr)
    point = new Point(&header->schema);

  if (laslibinterface->read_point(point))
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

  return true;
}

// In memory mode
bool LASRlasreader::process(LAS*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new LAS(header);

  streaming = false;

  progress->reset();
  progress->set_total(header->number_of_point_records);
  progress->set_prefix("read_las");

  Point p(&header->schema);

  while (laslibinterface->read_point(&p))
  {
    if (progress->interrupted()) break;

    if (p.inside_buffer(xmin, ymin, xmax, ymax, circular))
      p.set_buffered();

    if (!las->add_point(p)) return false;

    progress->update(laslibinterface->p_count());
    progress->show();
  }

  las->update_header();

  progress->done();

  if (verbose) print(" Number of point read %d\n", las->npoints);

  return true;
}

LASRlasreader::~LASRlasreader()
{
  if (laslibinterface)
  {
    laslibinterface->close();
    delete laslibinterface;
    laslibinterface = nullptr;
  }
}

void LASRlasreader::clear(bool)
{
  // Called at the end of the pipeline. We can delete the header
  if (streaming && header)
  {
    delete header;
    header = nullptr;
  }
}
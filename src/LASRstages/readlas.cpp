#include "readlas.h"

#include "LASio.h"
#include "lasmessage.hpp"
#include "print.h"

#include <string>

// Defined in lasmessage.cpp: indents the continuation lines of a multiline message
void format_message(std::string& messsage, unsigned multiline_ident, bool append_trailing_lf);

// LASlib message handler: forwards LASlib messages to lasR's thread safe print(), warning() and
// eprint() (Rprintf/REprintf if compiled with R). warning() and eprint() add their own
// 'WARNING: ' and 'ERROR: ' prefixes.
static void lasr_las_message_handler(LAS_MESSAGE_TYPE type, const char* msg, void* user_data)
{
  (void)user_data;
  std::string message(msg);
  switch (type)
  {
  case LAS_DEBUG:
  case LAS_VERY_VERBOSE:
  case LAS_VERBOSE:
  case LAS_INFO:
    print("%s\n", message.c_str());
    break;
  case LAS_WARNING:
    format_message(message, 9, true);  // strlen("WARNING: ")
    warning("%s", message.c_str());
    break;
  case LAS_SERIOUS_WARNING:
    message = "SERIOUS: " + message;
    format_message(message, 9, true);
    warning("%s", message.c_str());
    break;
  case LAS_ERROR:
    format_message(message, 7, true);  // strlen("ERROR: ")
    eprint("%s", message.c_str());
    break;
  case LAS_FATAL_ERROR:
    message = "FATAL: " + message;
    format_message(message, 7, true);
    eprint("%s", message.c_str());
    break;
  case LAS_QUIET:
    break;
  }
}

// Registered once at library load, before any LASlib call
static const bool lasr_las_message_handler_registered = []() {
  set_las_message_handler(&lasr_las_message_handler);
  return true;
}();

LASRlasreader::LASRlasreader()
{
  header = nullptr;
  lasio = nullptr;
  streaming = true;
}

bool LASRlasreader::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  // New chunk -> new reader for a new file. We can delete the previous reader and build a new one
  if (lasio)
  {
    lasio->close();
    delete lasio;
    lasio = nullptr;
  }

  lasio = new LASio();

  try
  {
    lasio->query(
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

bool LASRlasreader::process(Header*& header)
{
  // LASRlasreader is responsible for populating the header.
  // It is called first before LASRlasreader::process(Point) (streaming) or LASRlasreader::process(LAS) (in memory)
  // If the point is null then we create one Header. This object own the Header
  if (header != nullptr) return true;

  header = new Header;
  lasio->populate_header(header);

  this->header = header;

  return true;
}

// Streaming mode
bool LASRlasreader::process(Point*& point)
{
  if (point == nullptr)
    point = new Point(&header->schema);

  do
  {
    if (lasio->read_point(point))
    {
      if (point->inside_buffer(xmin, ymin, xmax, ymax, circular))
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
bool LASRlasreader::process(PointCloud*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new PointCloud(header);

  streaming = false;

  progress->reset();
  progress->set_total(header->number_of_point_records);
  progress->set_prefix("read_las");

  Point p(&header->schema);

  while (lasio->read_point(&p))
  {
    if (progress->interrupted()) break;
    if (pointfilter.filter(&p)) continue;
    if (p.inside_buffer(xmin, ymin, xmax, ymax, circular)) p.set_buffered();
    if (!las->add_point(p)) return false;

    progress->update(lasio->p_count());
    progress->show();
  }

  progress->done();
  if (verbose) print(" Number of point read %d\n", las->npoints);

  if (verbose) print("Building a spatial index\n");
  las->update_header();

  return true;
}

LASRlasreader::~LASRlasreader()
{
  if (lasio)
  {
    lasio->close();
    delete lasio;
    lasio = nullptr;
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
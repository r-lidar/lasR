#include "readept.h"

#include "EPTio.h"

LASReptreader::LASReptreader()
{
  header = nullptr;
  eptio = nullptr;
  streaming = true;
}

bool LASReptreader::set_chunk(Chunk& chunk)
{
  Stage::set_chunk(chunk);

  if (eptio)
  {
    eptio->close();
    delete eptio;
    eptio = nullptr;
  }

  eptio = new EPTio();

  if (ept_index) eptio->set_index(ept_index);
  chunk_strict_clip = chunk.strict_clip;
  catalog_xmax = chunk.catalog_xmax;
  catalog_ymax = chunk.catalog_ymax;

  try
  {
    eptio->query(
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

bool LASReptreader::process(Header*& header)
{
  if (header != nullptr) return true;

  header = new Header;

  try
  {
    eptio->populate_header(header);
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
bool LASReptreader::process(Point*& point)
{
  if (point == nullptr) point = new Point(&header->schema);

  while (true) {
    if (!eptio->read_point(point)) {
      delete point;
      point = nullptr;
      return true;
    }

    if (chunk_strict_clip) {
      const double bxmin = xmin - buffer;
      const double bxmax = xmax + buffer;
      const double bymin = ymin - buffer;
      const double bymax = ymax + buffer;
      const double px = point->get_x();
      const double py = point->get_y();
      if (px < bxmin || px > bxmax || py < bymin || py > bymax) continue;  // re-read
      bool owns_x = (px >= xmin) && (px < xmax || (px == xmax && xmax == catalog_xmax));
      bool owns_y = (py >= ymin) && (py < ymax || (py == ymax && ymax == catalog_ymax));
      if (!(owns_x && owns_y)) point->set_buffered();
    } else {
      if (point->inside_buffer(xmin, ymin, xmax, ymax, circular))
        point->set_buffered();
    }

    if (pointfilter.filter(point)) continue;  // re-read on filter reject
    return true;
  }
}

// In memory mode
bool LASReptreader::process(PointCloud*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new PointCloud(header);

  streaming = false;

  progress->reset();
  progress->set_total(header->number_of_point_records);
  progress->set_prefix("read_ept");

  Point p(&header->schema);

  while (eptio->read_point(&p)) {
    if (progress->interrupted()) break;
    if (pointfilter.filter(&p)) continue;

    if (chunk_strict_clip) {
      const double bxmin = xmin - buffer;
      const double bxmax = xmax + buffer;
      const double bymin = ymin - buffer;
      const double bymax = ymax + buffer;
      const double px = p.get_x();
      const double py = p.get_y();
      if (px < bxmin || px > bxmax || py < bymin || py > bymax) continue;
      bool owns_x = (px >= xmin) && (px < xmax || (px == xmax && xmax == catalog_xmax));
      bool owns_y = (py >= ymin) && (py < ymax || (py == ymax && ymax == catalog_ymax));
      if (!(owns_x && owns_y)) p.set_buffered();
    } else if (p.inside_buffer(xmin, ymin, xmax, ymax, circular)) {
      p.set_buffered();
    }

    if (!las->add_point(p)) return false;
    progress->update(eptio->p_count());
    progress->show();
  }

  progress->done();
  if (verbose) print(" Number of point read %d\n", las->npoints);

  if (verbose) print("Building a spatial index\n");
  las->update_header();

  return true;
}

LASReptreader::~LASReptreader()
{
  if (eptio)
  {
    eptio->close();
    delete eptio;
    eptio = nullptr;
  }
}

void LASReptreader::clear(bool)
{
  if (streaming && header)
  {
    delete header;
    header = nullptr;
  }
}

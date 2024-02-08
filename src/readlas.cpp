#include "readlas.h"
#include "lasreader.hpp"

LASRlasreader::LASRlasreader()
{
  lasreadopener = nullptr;
  lasreader = nullptr;
  lasheader = nullptr;
}

LASRlasreader::~LASRlasreader()
{
  // This happens when an error occurs in the pipeline otherwise
  // the reader and opener are close in clear()
  if (lasreader)
  {
    lasreader->close();
    delete lasreader;
    lasreader = nullptr;
  }

  if (lasreadopener)
  {
    delete lasreadopener;
  }

  lasheader = nullptr;
}

bool LASRlasreader::set_chunk(const Chunk& chunk)
{
  if (lasreader)
  {
    lasreader->close();
    delete lasreader;
    lasreader = nullptr;
    lasheader = nullptr;
  }
  if (lasreadopener)
  {
    delete lasreadopener;
    lasreadopener = nullptr;
  }

  const char* tmp = filter.c_str();
  int n = strlen(tmp)+1;
  char* filtercpy = (char*)malloc(n); memcpy(filtercpy, tmp, n);

  // The openner must survive to the reader otherwise there are some pointer invalidation.
  lasreadopener = new LASreadOpener;
  lasreadopener->set_merged(true);
  lasreadopener->set_stored(false);
  lasreadopener->set_buffer_size(chunk.buffer);
  lasreadopener->set_populate_header(true);
  lasreadopener->parse_str(filtercpy);

  free(filtercpy);

  for (auto& file : chunk.main_files) lasreadopener->add_file_name(file.c_str());
  for (auto& file : chunk.neighbour_files) lasreadopener->add_neighbor_file_name(file.c_str());

  if (chunk.shape == ShapeType::RECTANGLE)
    lasreadopener->set_inside_rectangle(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON);
  else if (chunk.shape == ShapeType::CIRCLE)
    lasreadopener->set_inside_circle((chunk.xmin+chunk.xmax)/2, (chunk.ymin+chunk.ymax)/2,  (chunk.xmax-chunk.xmin)/2 + chunk.buffer + EPSILON);
  else
    lasreadopener->set_inside_rectangle(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON);

  lasreader = lasreadopener->open();
  if (!lasreader)
  {
    // # nocov start
    char buffer[512];
    snprintf(buffer, 512, "LASlib internal error. Cannot open LASreader with %s\n", chunk.main_files[0].c_str());
    last_error = std::string(buffer);
    return false;
    // # nocov end
  }

  if (chunk.buffer == 0)
  {
    lasreader->header.clean_lasoriginal();
  }

  lasheader = &lasreader->header;

  return true;
}

bool LASRlasreader::process(LASheader*& header)
{
  header = this->lasheader;
  return true;
}

bool LASRlasreader::process(LASpoint*& point)
{
  if (lasreader->read_point())
    point = &lasreader->point;
  else
    point = nullptr;

  return true;
}

bool LASRlasreader::process(LAS*& las)
{
  if (las != nullptr) { delete las; las = nullptr; }
  if (las == nullptr) las = new LAS(lasheader);

  progress->reset();
  progress->set_total(lasreader->npoints);
  progress->set_prefix("read_las");

  while (lasreader->read_point())
  {
    las->add_point(lasreader->point);
    progress->update(lasreader->p_count);
    progress->show();
  }
  progress->done();

  return true;
}

void LASRlasreader::clear(bool last)
{
  // It is not possible to delete the reader and header here because the header
  // might be used later in the pipeline, typically by write_las. Instead set_chunk
  // handle the memory and the destructor terminate to free memory on last chunk.
  /*lasreader->close();
  delete lasreader;
  delete lasreadopener;
  lasreadopener = nullptr;
  lasreader = nullptr;
  lasheader = nullptr;*/
}
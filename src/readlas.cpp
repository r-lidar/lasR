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
  // # nocov start
  if (lasreader)
  {
    lasreader->close();
    delete lasreader;
    lasreader = nullptr;
    warning("internal error: lasreader was supposed to be nullptr in ~LASRlasreader. Please report");
  }

  if (lasreadopener)
  {
    delete lasreadopener;
    warning("internal error: lasreadopener was supposed to be nullptr in ~LASRlasreader. Please report");
  }
  // # nocov end

  lasheader = nullptr;
}

bool LASRlasreader::set_chunk(const Chunk& chunk)
{
  if (lasreadopener)
  {
    delete lasreadopener; // # nocov
    lasreadopener = nullptr; // # nocov
    warning("internal error: lasreadopener was supposed to be nullptr. Please report"); // # nocov
  }
  if (lasreader)
  {
    delete lasreader; // # nocov
    lasreader = nullptr; // # nocov
    warning("internal error: lasreader was supposed to be nullptr. Please report"); // # nocov
  }

  const char* tmp = filter.c_str();
  int n = strlen(tmp)+1;
  char* filtercpy = (char*)malloc(n); memcpy(filtercpy, tmp, n);

  lasreadopener = new LASreadOpener;
  lasreadopener->add_file_name(chunk.main_file.c_str());
  lasreadopener->set_merged(true);
  lasreadopener->set_stored(false);
  lasreadopener->set_buffer_size(chunk.buffer);
  lasreadopener->set_populate_header(true);
  lasreadopener->parse_str(filtercpy);

  if (chunk.shape == ShapeType::RECTANGLE)
    lasreadopener->set_inside_rectangle(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON);
  else if (chunk.shape == ShapeType::CIRCLE)
    lasreadopener->set_inside_circle((chunk.xmin+chunk.xmax)/2, (chunk.ymin+chunk.ymax)/2,  (chunk.xmax-chunk.xmin)/2 + chunk.buffer + EPSILON);
  else
    lasreadopener->set_inside_rectangle(chunk.xmin - chunk.buffer - EPSILON, chunk.ymin - chunk.buffer- EPSILON, chunk.xmax + chunk.buffer + EPSILON, chunk.ymax + chunk.buffer + EPSILON);

  for (auto& file : chunk.neighbour_files) lasreadopener->add_neighbor_file_name(file.c_str());

  lasreader = lasreadopener->open();
  if (!lasreader)
  {
    last_error = "LASlib internal error. Cannot open LASreader."; // # nocov
    return false; // # nocov
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
    (*progress)++;
    progress->show();
  }
  progress->done();

  return true;
}

void LASRlasreader::clear(bool last)
{
  lasreader->close();
  delete lasreader;
  delete lasreadopener;
  lasreadopener = nullptr;
  lasreader = nullptr;
  lasheader = nullptr;
}
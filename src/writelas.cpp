#include "writelas.h"

#include "laswriter.hpp"

LASRlaswriter::LASRlaswriter(double xmin, double ymin, double xmax, double ymax, bool keep_buffer)
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
  this->keep_buffer = keep_buffer;
  laswriter = nullptr;
  lasheader = nullptr;
  merged = false;
}

LASRlaswriter::~LASRlaswriter()
{
  if (laswriter)
  {
    // # nocov start
    warning("internal error: please report, a LASwriter is still opened when destructing LASRlaswriter. The LAS or LAZ file written may be corrupted\n");
    laswriter->close();
    delete laswriter;
    laswriter = nullptr;
    // # nocov end
  }
}

void LASRlaswriter::set_input_file_name(std::string file)
{
  ofile = template_filename;
  ifile = file;
  size_t pos = ofile.find('*');

  if (pos != std::string::npos)
  {
    ofile.replace(pos, 1, ifile);
  }
}

void LASRlaswriter::set_output_file(std::string file)
{
  template_filename = file;
  size_t pos = file.find('*');
  if (pos == std::string::npos)
  {
    merged = true;
  }
}

bool LASRlaswriter::process(LASpoint*& p)
{
  if (!laswriter)
  {
    LASwriteOpener laswriteopener;
    laswriteopener.set_file_name(ofile.c_str());
    laswriter = laswriteopener.open(lasheader);

    if (!laswriter)
    {
      last_error = "LASlib internal error. Cannot open LASwriter."; // # nocov
      return false; // # nocov
    }

    written.push_back(ofile);
  }

  if (keep_buffer || !p->inside_buffer(xmin, ymin, xmax, ymax, circular))
  {
    if (!lasfilter.filter(p))
    {
      laswriter->write_point(p);
      laswriter->update_inventory(p);
    }
  }

  return true;
}

bool LASRlaswriter::process(LAS*& las)
{
  progress->reset();
  progress->set_prefix("Write LAS");
  progress->set_total(las->npoints);

  LASpoint* p;
  while (las->read_point())
  {
    p = &las->point;
    if (!process(p))
      return false; // # nocov

    (*progress)++;
    progress->show();
  }

  progress->done();
  return true;
}

void LASRlaswriter::set_header(LASheader*& header)
{
  lasheader = header;
}

void LASRlaswriter::clear(bool last)
{
  if (!merged || last)
  {
    if (laswriter)
    {
      laswriter->update_header(lasheader, true);
      laswriter->close();
      delete laswriter;
      laswriter = nullptr;
    }
  }
}
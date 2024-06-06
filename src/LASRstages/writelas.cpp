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
  size_t pos = file.find('*');

  if (pos == std::string::npos)
  {
    merged = true;
  }

  return true;
}

bool LASRlaswriter::process(LASpoint*& p)
{
  if (p->get_withheld_flag() != 0) return true;

  // No writer initialized? Create a writer.
  if (!laswriter)
  {
    LASwriteOpener laswriteopener;
    laswriteopener.set_file_name(ofile.c_str());
    laswriter = laswriteopener.open(lasheader);

    offsets[0] = p->quantizer->x_offset;
    offsets[1] = p->quantizer->y_offset;
    offsets[2] = p->quantizer->z_offset;
    scales[0] = p->quantizer->x_scale_factor;
    scales[1] = p->quantizer->y_scale_factor;
    scales[2] = p->quantizer->z_scale_factor;

    if (!laswriter)
    {
      last_error = "LASlib internal error. Cannot open LASwriter."; // # nocov
      return false; // # nocov
    }

    written.push_back(ofile);
  }

  //  If the point in not in the buffer we can write it
  if (keep_buffer || !p->inside_buffer(xmin, ymin, xmax, ymax, circular))
  {
    if (!lasfilter.filter(p))
    {
      //print("Before %d %d %d \n", p->get_X(), p->get_Y(), p->get_Z());

      // If we write in a merged file the points may come from different file formats
      if (merged)
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
      }

      //print("After %d %d %d \n\n", p->get_X(), p->get_Y(), p->get_Z());
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
    if (progress->interrupted()) break;
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
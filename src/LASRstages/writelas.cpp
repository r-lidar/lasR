#include "writelas.h"

#include "laswriter.hpp"

LASRlaswriter::LASRlaswriter()
{
  laswriter = nullptr;
  lasheader = nullptr;
  point = nullptr;
  reset_accessor();
}

bool LASRlaswriter::set_parameters(const nlohmann::json& stage)
{
  keep_buffer = stage.value("keep_buffer", false);
  return true;
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

  if (lasheader)
  {
    delete lasheader;
    lasheader = nullptr;
  }

  if (point)
  {
    delete point;
    point = nullptr;
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

  //  If the point in not in the buffer we can write it
  if (keep_buffer || !p->inside_buffer(xmin, ymin, xmax, ymax, circular))
  {
    if (!pointfilter.filter(p))
    {
      //print("Before %d %d %d \n", p->get_X(), p->get_Y(), p->get_Z());

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
      point->set_x(p->get_x());
      point->set_y(p->get_y());
      point->set_z(p->get_z());
      point->set_intensity(intensity(p));
      point->set_return_number(returnnumber(p));
      point->set_number_of_returns(numberofreturns(p));
      point->set_user_data(userdata(p));
      point->set_point_source_ID(psid(p));
      point->set_classification(classification(p));
      point->set_scan_angle(scanangle(p));
      point->set_gps_time(gpstime(p));
      point->set_extended_scanner_channel(scannerchannel(p));
      point->set_R(red(p));
      point->set_G(green(p));
      point->set_B(blue(p));
      point->set_NIR(nir(p));

      int i = 0;
      for (auto attribute : p->schema->attributes)
      {
        if (lascoreattributes.count(attribute.name) == 0)
        {
          point->set_attribute(i, p->data + attribute.offset);
          i++;
        }
      }

      laswriter->write_point(point);
      laswriter->update_inventory(point);
    }
  }

  return true;
}

bool LASRlaswriter::process(LAS*& las)
{
  progress->reset();
  progress->set_prefix("Write LAS");
  progress->set_total(las->npoints);

  Point* p;
  while (las->read_point())
  {
    p = &las->p;
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

  // If we still have a LASheader this means that we are merging multiple files
  // We don't need to create a new LASheader.
  if (lasheader) return;

  int version_minor = 2;

  bool has_gps = header->schema.find_attribute("gpstime") != nullptr;
  bool has_rgb = header->schema.find_attribute("R") != nullptr;
  bool has_nir = header->schema.find_attribute("NIR") != nullptr;

  lasheader = new LASheader();
  lasheader->file_source_ID       = 0;
  lasheader->version_major        = 1;
  lasheader->version_minor        = version_minor;
  lasheader->header_size          = LAS::get_header_size(version_minor);
  lasheader->offset_to_point_data = LAS::get_header_size(version_minor);
  lasheader->file_creation_year   = 0;
  lasheader->file_creation_day    = 0;
  lasheader->point_data_format    = LAS::guess_point_data_format(has_gps, has_rgb, has_nir);
  lasheader->point_data_record_length = LAS::get_point_data_record_length(lasheader->point_data_format);
  lasheader->x_scale_factor       = header->schema.attributes[0].scale_factor;
  lasheader->y_scale_factor       = header->schema.attributes[1].scale_factor;
  lasheader->z_scale_factor       = header->schema.attributes[2].scale_factor;
  lasheader->x_offset             = header->schema.attributes[0].offset;
  lasheader->y_offset             = header->schema.attributes[1].offset;
  lasheader->z_offset             = header->schema.attributes[2].offset;
  lasheader->number_of_point_records = 0;
  lasheader->min_x                = xmin;
  lasheader->min_y                = ymin;
  lasheader->max_x                = xmax;
  lasheader->max_y                = ymax;

  reset_accessor();

  int num_extrabytes = 0;
  for (auto attribute : header->schema.attributes)
  {
    if (lascoreattributes.count(attribute.name) == 0)
    {
      LASattribute attr(attribute.type-1, attribute.name.c_str(), attribute.description.c_str());
      attr.set_scale(attribute.scale_factor);
      attr.set_offset(attribute.value_offset);
      lasheader->add_attribute(attr);
      num_extrabytes++;
    }
  }

  lasheader->update_extra_bytes_vlr();
  lasheader->point_data_record_length += lasheader->get_attributes_size();

  if (!crs.get_wkt().empty())
  {
    std::string wkt = crs.get_wkt();
    lasheader->set_global_encoding_bit(4);
    lasheader->set_geo_ogc_wkt(wkt.size(), wkt.c_str(), false);
  }

  point = new LASpoint;
  point->init(lasheader, lasheader->point_data_format, lasheader->point_data_record_length, lasheader);
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
    if (laswriter)
    {
      laswriter->update_header(lasheader, true);
      laswriter->close();
      delete laswriter;
      laswriter = nullptr;
    }

    if (point)
    {
      delete point;
      point = nullptr;
    }

    if (lasheader)
    {
      delete lasheader;
      lasheader = nullptr;
    }
  }

  reset_accessor();
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

void LASRlaswriter::reset_accessor()
{
  intensity = AttributeHandler("Intensity");
  returnnumber = AttributeHandler("ReturnNumber");
  numberofreturns = AttributeHandler("NumberOfReturns");
  userdata = AttributeHandler("UserData");
  classification = AttributeHandler("Classification");
  psid = AttributeHandler("PointSourceID");
  scanangle = AttributeHandler("ScanAngle");
  gpstime = AttributeHandler("gpstime");
  scannerchannel = AttributeHandler("ScannerChannel");
  red = AttributeHandler("R");
  green = AttributeHandler("G");
  blue = AttributeHandler("B");
  nir = AttributeHandler("NIR");
  extrabytes.clear();
}


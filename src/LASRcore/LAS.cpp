#include "LAS.h"
#include "GridPartition.h"
#include "Raster.h"
#include "macros.h"
#include "error.h"

#include "lasdefinitions.hpp"
#include "lasfilter.hpp"
#include "lastransform.hpp"

LAS::LAS(LASheader* header)
{
  this->header = header;
  this->own_header = false;

  buffer = 0;
  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, 2);

  npoints = 0;
  capacity = 0;
  current_point = 0;
  next_point = 0;

  read_started = false;

  // For spatial indexing
  current_interval = 0;
  shape = nullptr;
  inside = false;

  // Initialize the good point format
  point.init(header, header->point_data_format, header->point_data_record_length, header);

  // This fixes #2 and troubles with add_extrabytes but I don't know exactly why except
  // it is a matter of item in the compressor
  delete header->laszip;
  header->laszip = 0;
}

LAS::LAS(const Raster& raster)
{
  own_header = true;

  buffer = 0;

  npoints = 0;
  capacity = 0;
  current_point = 0;
  next_point = 0;

  read_started = false;

  // For spatial indexing
  current_interval = 0;
  shape = nullptr;
  inside = false;

  // Convert the raster to a LAS
  header = new LASheader;
  header->file_source_ID       = 0;
  header->version_major        = 1;
  header->version_minor        = 2;
  header->header_size          = 227;
  header->offset_to_point_data = 227;
  header->file_creation_year   = 0;
  header->file_creation_day    = 0;
  header->point_data_format    = 0;
  header->x_scale_factor       = 0.01;
  header->y_scale_factor       = 0.01;
  header->z_scale_factor       = 0.01;
  header->x_offset             = raster.get_full_extent()[0];
  header->y_offset             = raster.get_full_extent()[1];
  header->z_offset             = 0;
  header->number_of_point_records = raster.get_ncells();
  header->min_x                = raster.get_xmin()-raster.get_xres()/2;
  header->min_y                = raster.get_ymin()-raster.get_yres()/2;
  header->max_x                = raster.get_xmax()+raster.get_xres()/2;
  header->max_y                = raster.get_ymax()-raster.get_yres()/2;
  header->point_data_record_length = 20;

  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, raster.get_xres()*4);

  point.init(header, header->point_data_format, header->point_data_record_length, header);

  float nodata = raster.get_nodata();

  for (int i = 0 ; i < raster.get_ncells() ; i++)
  {
    float z = raster.get_value(i);

    if (std::isnan(z) || z == nodata) continue;

    double x = raster.x_from_cell(i);
    double y = raster.y_from_cell(i);

    point.set_x(x);
    point.set_y(y);
    point.set_z(z);

    add_point(point);
  }

  header->number_of_point_records = npoints;
}

LAS::~LAS()
{
  if (buffer) free(buffer);
  if (own_header) delete header;
  clean_index();
}

bool LAS::add_point(const LASpoint& p)
{
  if (buffer == 0)
  {
    capacity = 100000;
    buffer = (unsigned char*)malloc(capacity * point.total_point_size);
    if (buffer == 0)
    {
      eprint("Memory allocation failed\n"); // # nocov
      return false;                         // # nocov
    }
  }

  if (npoints == I32_MAX)
  {
    eprint("LASR cannot stores more than %d points", I32_MAX); // # nocov
    return false;                                              // # nocov
  }

  if (npoints == capacity)
  {
    uint64_t capacity_max = MAX(header->number_of_point_records, header->extended_number_of_point_records);

    // This may happens if the header is not properly populated
    if (npoints >= capacity_max)
      capacity_max = capacity*2; // # nocov

    if (capacity_max < (uint64_t)capacity*2)
      capacity = capacity_max;
    else
      capacity *= 2;

    buffer = (unsigned char*)realloc((void*)buffer, capacity * point.total_point_size);

    if (buffer == 0)
    {
      eprint("Memory reallocation failled\n"); // # nocov
      return false;                            // # nocov
    }
  }

  point = p; // Point format conversion
  point.copy_to(buffer + npoints * point.total_point_size);

  if (index) index->insert(point.get_x(), point.get_y());

  npoints++;

  return true;
}

bool LAS::seek(int pos)
{
  if (pos < 0 || pos >= npoints) return false;
  current_point = pos;
  next_point = pos;
  next_point++;
  point.copy_from(buffer + current_point * point.total_point_size);
  return true;
}

// Thread safe
void LAS::get_xyz(int pos, double* xyz) const
{
  unsigned char* buf = buffer + pos * point.total_point_size;
  unsigned int X = *((unsigned int*)buf);
  unsigned int Y = *((unsigned int*)(buf+4));
  unsigned int Z = *((unsigned int*)(buf+8));
  xyz[0] = point.quantizer->get_x(X);
  xyz[1] = point.quantizer->get_y(Y);
  xyz[2] = point.quantizer->get_z(Z);
  return;
}

bool LAS::read_point(bool include_withhelded)
{
  // Query the ids of the points if we did not start reading yet
  if (!read_started)
  {
    current_interval = 0;

    if (!inside)
      intervals_to_read.push_back({0, npoints-1});
    else if (inside && shape)
      index->query(shape->xmin(), shape->ymin(), shape->xmax(), shape->ymax(), intervals_to_read);
    else
    {
      // Nothing to do.
    }


    if (intervals_to_read.size() == 0)
      return false;

    next_point = intervals_to_read[0].start;
    read_started = true;
  }

  // If the interval index is beyond the list of intervals we have read everything
  if (current_interval >= (int)intervals_to_read.size())
  {
    clean_query();
    return false;
  }

  do
  {
    // If the interval index is beyond the list of intervals we have read everything
    if (current_interval >= (int)intervals_to_read.size())
    {
      clean_query();
      return false;
    }

    current_point = next_point;
    point.copy_from(buffer + current_point * point.total_point_size);
    next_point++;

    // If the new current point is not in the current interval we switch to next interval
    if (next_point > intervals_to_read[current_interval].end)
    {
      current_interval++;
      if (current_interval < (int)intervals_to_read.size())
        next_point = intervals_to_read[current_interval].start;
    }

    if (shape)
    {
      if (shape->contains(point.get_x(), point.get_y()))
      {
        if (include_withhelded || point.get_withheld_flag() == 0) return true;
      }
    }
    else
    {
      if (include_withhelded || point.get_withheld_flag() == 0) return true;
    }
  } while (true);

  return true;
}

void LAS::update_point()
{
  point.copy_to(buffer + current_point * point.total_point_size);
}


void LAS::remove_point()
{
  point.set_withheld_flag(1);
  update_point();
}

// Thread safe
bool LAS::query(const Shape* const shape, std::vector<PointLAS>& addr, LASfilter* const lasfilter, LAStransform* const lastransform) const
{
  LASpoint p;
  p.init(point.quantizer, point.num_items, point.items, point.attributer);

  addr.clear();

  std::vector<Interval> intervals;
  index->query(shape->xmin(), shape->ymin(), shape->xmax(), shape->ymax(), intervals);

  if (intervals.size() == 0) return false;

  for (const auto& interval : intervals)
  {
    for (int i = interval.start ; i <= interval.end ; i++)
    {
      p.copy_from(buffer + i * p.total_point_size);

      if (lasfilter && lasfilter->filter(&p)) continue;

      if (point.get_withheld_flag() == 0 && shape->contains(p.get_x(), p.get_y()))
      {
        if (lastransform) lastransform->transform(&p);

        PointLAS pl(&p);
        pl.FID = i;
        addr.push_back(pl);
      }
    }
  }

  return addr.size() > 0;
}

// Thread safe
bool LAS::query(const std::vector<Interval>& intervals, std::vector<PointLAS>& addr, LASfilter* const lasfilter, LAStransform* const lastransform) const
{
  LASpoint p;
  p.init(point.quantizer, point.num_items, point.items, point.attributer);

  addr.clear();

  if (intervals.size() == 0) return false;

  for (const auto& interval : intervals)
  {
    for (int i = interval.start ; i <= interval.end ; i++)
    {
      p.copy_from(buffer + i * p.total_point_size);

      if (lasfilter && lasfilter->filter(&p)) continue;

      if (point.get_withheld_flag() == 0)
      {
        if (lastransform) lastransform->transform(&p);

        PointLAS pl(&p);
        pl.FID = i;
        addr.push_back(pl);
      }
    }
  }

  return addr.size() > 0;
}

// Thread safe
bool LAS::get_point(int pos, PointLAS& pt, LASfilter* const lasfilter, LAStransform* const lastransform) const
{
  LASpoint p;
  p.init(point.quantizer, point.num_items, point.items, point.attributer);
  p.copy_from(buffer + pos * p.total_point_size);
  if (p.get_withheld_flag() != 0) return false;
  if (lastransform) lastransform->transform(&p);
  if (lasfilter && lasfilter->filter(&p)) return false;
  pt.copy(&p);
  return true;
}

void LAS::set_inside(Shape* shape)
{
  clean_query();
  inside = true;
  this->shape = shape;
}

void LAS::set_intervals_to_read(const std::vector<Interval>& intervals)
{
  clean_query();
  inside = true;
  intervals_to_read = intervals;
}

bool LAS::add_attribute(int data_type, const std::string& name, const std::string& description, double scale, double offset)
{
  data_type--;
  bool has_scale = scale != 1;
  bool has_offset = offset != 0;
  bool has_no_data = false;
  bool has_min = false;
  bool has_max = false;
  double min = 0;
  double max = 0;
  double no_data = -99999.0;

  LASattribute attribute(data_type, name.c_str(), description.c_str());

  if (has_scale) attribute.set_scale(scale);
  if (has_offset) attribute.set_offset(offset);

  /*if (has_no_data)
  {
    switch(data_type)
    {
    case UCHAR:     attribute.set_no_data(U8_CLAMP(U8_QUANTIZE(no_data))); break;
    case CHAR:      attribute.set_no_data(I8_CLAMP(I8_QUANTIZE(no_data))); break;
    case USHORT:    attribute.set_no_data(U16_CLAMP(U16_QUANTIZE(no_data))); break;
    case SHORT:     attribute.set_no_data(I16_CLAMP(I16_QUANTIZE(no_data))); break;
    case ULONG:     attribute.set_no_data(U32_CLAMP(U32_QUANTIZE(no_data))); break;
    case LONG:      attribute.set_no_data(I32_CLAMP(I32_QUANTIZE(no_data))); break;
    case ULONGLONG: attribute.set_no_data(U64_QUANTIZE(no_data)); break;
    case LONGLONG:  attribute.set_no_data(I64_QUANTIZE(no_data)); break;
    case FLOAT:     attribute.set_no_data((float)(no_data)); break;
    case DOUBLE:    attribute.set_no_data(no_data); break;
    }
  }

  if(has_min)
  {
    switch(data_type)
    {
    case UCHAR:     attribute.set_min(U8_CLAMP(U8_QUANTIZE(min))); break;
    case CHAR:      attribute.set_min(I8_CLAMP(I8_QUANTIZE(min))); break;
    case USHORT:    attribute.set_min(U16_CLAMP(U16_QUANTIZE(min))); break;
    case SHORT:     attribute.set_min(I16_CLAMP(I16_QUANTIZE(min))); break;
    case ULONG:     attribute.set_min(U32_CLAMP(U32_QUANTIZE(min))); break;
    case LONG:      attribute.set_min(I32_CLAMP(I32_QUANTIZE(min))); break;
    case ULONGLONG: attribute.set_min(U64_QUANTIZE(min)); break;
    case LONGLONG:  attribute.set_min(I64_QUANTIZE(min)); break;
    case FLOAT:     attribute.set_min((float)(min)); break;
    case DOUBLE:    attribute.set_min(min); break;
    }
  }

  // set max value if option set
  if(has_max)
  {
    switch(data_type)
    {
    case UCHAR: attribute.set_max(U8_CLAMP(U8_QUANTIZE(max))); break;
    case CHAR: attribute.set_max(I8_CLAMP(I8_QUANTIZE(max))); break;
    case USHORT: attribute.set_max(U16_CLAMP(U16_QUANTIZE(max))); break;
    case SHORT: attribute.set_max(I16_CLAMP(I16_QUANTIZE(max))); break;
    case ULONG: attribute.set_max(U32_CLAMP(U32_QUANTIZE(max))); break;
    case LONG: attribute.set_max(I32_CLAMP(I32_QUANTIZE(max))); break;
    case ULONGLONG: attribute.set_max(U64_QUANTIZE(max)); break;
    case LONGLONG: attribute.set_max(I64_QUANTIZE(max)); break;
    case FLOAT: attribute.set_max((float)(max)); break;
    case DOUBLE: attribute.set_max(max); break;
    }
  }*/

  int attr_index = header->add_attribute(attribute);
  if (attr_index == -1)
  {
    eprint("LASlib internal error: add_attribute failed"); // # nocov
    return false; // # nocov
  }

  header->update_extra_bytes_vlr();
  header->point_data_record_length += attribute.get_size();

  return update_point_and_buffer();
}

/*void LAS::set_index(bool index)
{
  clean_index();
  index = new GridPartition(lasheader.min_x, lasheader.min_y, lasheader.max_x, lasheader.max_y, 10);
  while (read_point()) index->insert(point);
}

void LAS::set_index(float res)
{
  clean_index();
  index = new GridPartition(lasheader.min_x, lasheader.min_y, lasheader.max_x, lasheader.max_y, res);
  while (read_point()) index->insert(point);
}*/


bool LAS::add_rgb()
{
  int pdf = header->point_data_format;
  int target;

  if (pdf == 2 || pdf == 3 || pdf == 5 || pdf == 7 || pdf == 8 || pdf == 10)
    return true;

  if (pdf == 0)      target = 2;
  else if (pdf == 1) target = 3;
  else if (pdf == 4) target = 5;
  else if (pdf == 6) target = 7;
  else if (pdf == 9) target = 10;

  header->point_data_format = target;
  header->point_data_record_length += 3*2; // 3 x 16 bits = 3 x 2 bytes

  if (target == 10)
    header->point_data_record_length += 2; // 2 bytes for NIR

  return update_point_and_buffer();
}

void LAS::clean_index()
{
  clean_query();
  if (index) delete index;
}

void LAS::clean_query()
{
  current_interval = 0;
  shape = nullptr;
  inside = false;
  read_started = false;
  intervals_to_read.clear();
}

bool LAS::is_attribute_loadable(int index)
{
  if (index < 0) return false;
  if (header->number_attributes-1 < index) return false;

  int data_type = point.attributer->attributes[index].data_type;

  if (data_type == ULONG || data_type == ULONGLONG || data_type == LONGLONG)
  {
    warning("unsigned 32 bits integers and 64 bits integers are not supported in R");
    return false;
  }

  return true;
}

bool LAS::update_point_and_buffer()
{
  LASpoint new_point;
  new_point.init(header, header->point_data_format, header->point_data_record_length, header);

  if (capacity * point.total_point_size < npoints * new_point.total_point_size)
  {
    buffer = (unsigned char*)realloc((void*)buffer, npoints * new_point.total_point_size);
  }

  if (buffer == 0)
  {
    last_error = "LAS::update_point_and_buffer(): memory allocation failed"; // # nocov
    return false; // # nocov
  }

  for (int i = npoints-1 ; i >= 0 ; --i)
  {
    seek(i);
    new_point = point;
    new_point.copy_to(buffer + i * new_point.total_point_size);
  }

  point = LASpoint();
  point.init(header, header->point_data_format, header->point_data_record_length, header);

  return true;
}

LAStransform* LAS::make_z_transformer(const std::string& use_attribute)
{
  if (use_attribute == "Intensity")
  {
    char buffer[] = "-copy_intensity_into_z";
    LAStransform* lastransform = new LAStransform();
    lastransform->parse(buffer);
    return lastransform;
  }
  else
  {
    int attr_index = header->get_attribute_index(use_attribute.c_str());
    if (attr_index == -1) return nullptr;

    char buffer[64];
    snprintf(buffer, sizeof(buffer), "-copy_attribute_into_z %d", attr_index);
    LAStransform* lastransform = new LAStransform();
    lastransform->parse(buffer);
    return lastransform;
  }
}
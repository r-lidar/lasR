#include "LAS.h"
#include "GridPartition.h"
#include "Raster.h"
#include "macros.h"
#include "error.h"

#include "lasdefinitions.hpp"
#include "lasfilter.hpp"
#include "lastransform.hpp"
#include "lasutility.hpp"

#include <algorithm>

LAS::LAS(LASheader* header)
{
  this->header = header;
  this->own_header = false;

  // Point cloud storage
  buffer = NULL;
  npoints = 0;
  capacity = 0;

  current_point = 0;
  next_point = 0;
  read_started = false;

  // For spatial indexing
  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, 2);
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

  // Point cloud storage
  buffer = NULL;
  npoints = 0;
  capacity = 0;

  current_point = 0;
  next_point = 0;
  read_started = false;

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

  // For spatial indexing
  current_interval = 0;
  shape = nullptr;
  inside = false;
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
  if (buffer == NULL)
  {
    capacity = 100000 * point.total_point_size;
    if (!alloc_buffer()) return false;
  }

  if (npoints == I32_MAX)
  {
    last_error = "LASR cannot stores more than 2147483647 points"; // # nocov
    return false;                                                  // # nocov
  }

  // Realloc memory and increase buffer size if needed
  size_t required_capacity = npoints*point.total_point_size;
  if (required_capacity == capacity)
  {
    size_t capacity_max = get_true_number_of_points()*point.total_point_size;

    // This may happens if the header is not properly populated
    if (required_capacity >= capacity_max)
      capacity_max = capacity*2; // # nocov

    if (capacity_max < capacity*2)
      capacity = capacity_max;
    else
      capacity *= 2;

    if (!realloc_buffer()) return false;
  }

  point = p; // Point format conversion (useful??)
  point.copy_to(buffer + npoints * point.total_point_size);
  npoints++;

  index->insert(point.get_x(), point.get_y());

  return true;
}

bool LAS::seek(size_t pos)
{
  if (pos >= npoints)
  {
    last_error = "seek out of bounds"; // # nocov
    return false; // # nocov
  }

  current_point = pos;
  next_point = pos;
  next_point++;
  point.copy_from(buffer + current_point * point.total_point_size);
  return true;
}

// Thread safe and fast
bool LAS::get_xyz(size_t pos, double* xyz) const
{
  if (pos >= npoints)
  {
    last_error = "seek out of bounds"; // # nocov
    return false; // # nocov
  }

  unsigned char* buf = buffer + pos * point.total_point_size;
  xyz[0] = get_x(buf);
  xyz[1] = get_y(buf);
  xyz[2] = get_z(buf);
  return true;
}

bool LAS::read_point(bool include_withhelded)
{
  if (npoints == 0) return false; // Fix #40

  // Query the ids of the points if we did not start reading yet
  if (!read_started)
  {
    current_interval = 0;

    if (!inside)
      intervals_to_read.push_back({0, (int)npoints-1});
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

bool LAS::delete_withheld()
{
  clean_index();
  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, 2);

  int j = 0;
  for (int i = 0 ; i < npoints ; i++)
  {
    seek(i);
    if (point.get_withheld_flag() == 0)
    {
      point.copy_to(buffer + j * point.total_point_size);
      index->insert(point.get_x(), point.get_y());
      j++;
    }
  }

  double ratio = (double)j/(double)npoints;
  npoints = j;

  if (ratio < 0.5)
  {
    capacity = npoints*point.total_point_size;
    if (!realloc_buffer()) return false;
  }

  return true;
}

// Static functions defined for C qsort in LAS::sort
static inline double get_gps_time_extended(const unsigned char* buf) { return *((const double*)&buf[22]); };
static inline double get_gps_time_legacy(const unsigned char* buf) { return *((const double*)&buf[20]); };
static inline unsigned char get_scanner_channel(const unsigned char* buf) { return (buf[15] >> 4) & 0x03; };
static inline unsigned char get_return_number(const unsigned char* buf) { return buf[14] & 0x0F; };
static int compare_buffers(const void *a, const void *b)
{
  if (get_gps_time_extended((const unsigned char*)a) < get_gps_time_extended((const unsigned char*)b)) return -1;
  if (get_gps_time_extended((const unsigned char*)a) > get_gps_time_extended((const unsigned char*)b)) return 1;
  if (get_scanner_channel((const unsigned char*)a) < get_scanner_channel((const unsigned char*)b)) return -1;
  if (get_scanner_channel((const unsigned char*)a) > get_scanner_channel((const unsigned char*)b)) return 1;
  if (get_return_number((const unsigned char*)a) < get_return_number((const unsigned char*)b)) return -1;
  if (get_return_number((const unsigned char*)a) > get_return_number((const unsigned char*)b)) return 1;
  return 0;
}
static int compare_buffers_nochannel(const void *a, const void *b)
{
  if (get_gps_time_legacy((const unsigned char*)a) < get_gps_time_legacy((const unsigned char*)b)) return -1;
  if (get_gps_time_legacy((const unsigned char*)a) > get_gps_time_legacy((const unsigned char*)b)) return 1;
  if (get_return_number((const unsigned char*)a) < get_return_number((const unsigned char*)b)) return -1;
  if (get_return_number((const unsigned char*)a) > get_return_number((const unsigned char*)b)) return 1;
  return 0;
}
static int compare_buffers_nogps(const void *a, const void *b)
{
  if (get_return_number((const unsigned char*)a) < get_return_number((const unsigned char*)b)) return -1;
  if (get_return_number((const unsigned char*)a) > get_return_number((const unsigned char*)b)) return 1;
  return 0;
}

bool LAS::sort()
{
  if (point.have_gps_time && point.is_extended_point_type())
    qsort((void*)buffer, npoints, point.total_point_size, compare_buffers);
  else if (point.have_gps_time)
    qsort((void*)buffer, npoints, point.total_point_size, compare_buffers_nochannel);
  else
    qsort((void*)buffer, npoints, point.total_point_size, compare_buffers_nogps);

  reindex();

  return true;
}

bool LAS::sort(const std::vector<int>& order)
{
  std::vector<bool> visited(npoints, false);
  char* temp = (char*)malloc(point.total_point_size);
  size_t chunk_size = point.total_point_size;

  for (size_t i = 0; i < npoints; ++i)
  {
    if (visited[i] || order[i] == i) continue;

    size_t current = i;

    memcpy(temp, buffer + current * chunk_size, chunk_size);

    while (!visited[current])
    {
      visited[current] = true;
      size_t next = order[current];

      if (next != i)
        memcpy(buffer + current * chunk_size, buffer + next * chunk_size, chunk_size);
      else
        memcpy(buffer + current * chunk_size, temp, chunk_size);

      current = next;
    }
  }

  free(temp);

  reindex();

  return true;
}

void LAS::update_header()
{
  LASinventory inventory;
  while (read_point()) inventory.add(&point);
  inventory.update_header(header);
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

      if (p.get_withheld_flag() == 0 && shape->contains(p.get_x(), p.get_y()))
      {
        if (lastransform) lastransform->transform(&p);

        PointLAS pl(&p);
        pl.FID = i;
        addr.push_back(std::move(pl));
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

      if (p.get_withheld_flag() == 0)
      {
        if (lastransform) lastransform->transform(&p);

        PointLAS pl(&p);
        pl.FID = i;
        addr.push_back(std::move(pl));
      }
    }
  }

  return addr.size() > 0;
}

// Thread safe
bool LAS::knn(const double* xyz, int k, double radius_max, std::vector<PointLAS>& res,  LASfilter* const lasfilter, LAStransform* const lastransform) const
{
  double x = xyz[0];
  double y = xyz[1];
  double z = xyz[2];

  LASpoint p;
  p.init(point.quantizer, point.num_items, point.items, point.attributer);

  double area = (header->max_x-header->min_x)*(header->max_y-header->min_y);
  double density = get_true_number_of_points() / area;
  double radius  = std::sqrt((double)k / (density * 3.14)) * 1.5;

  int n = 0;
  std::vector<Interval> intervals;
  if (radius < radius_max)
  {
    // While we do not have k points or we did not reached the max radius search we increment the radius
    while (n < k && n < npoints && radius <= radius_max)
    {
      intervals.clear();
      index->query(x-radius, y-radius, x+radius, y+radius, intervals);

      // In lasR we query intervals not points so we need to count the number of points in the interval
      n = 0; for (const auto& interval : intervals) n += interval.end - interval.start + 1;

      // If we have more than k points we may not have the knn because of the filter and withhelded points
      // we need to fetch the points to actually count them
      if (n >= k)
      {
        n = 0;
        Sphere s(x,y,z, radius);
        for (const auto& interval : intervals)
        {
          for (int i = interval.start ; i <= interval.end ; i++)
          {
            p.copy_from(buffer + i * p.total_point_size);
            if (p.get_withheld_flag() != 0) continue;
            if (lasfilter && lasfilter->filter(&p)) continue;
            if (!s.contains(p.get_x(), p.get_y(), p.get_z())) continue;
            n++;
          }
        }
      }

      // After fetching the point
      if (n < k) radius *= 1.5;
    }
  }

  // We incremented the radius until we get k points. If the radius is bigger than the max radius we use radius = max radius
  // and we may not have k points.
  if (radius >= radius_max) radius = radius_max;

  // We perform the query for real
  intervals.clear();
  index->query(x-radius, y-radius, x+radius, y+radius, intervals);

  res.clear();
  Sphere s(x,y,z, radius);
  for (const auto& interval : intervals)
  {
    for (int i = interval.start ; i <= interval.end ; i++)
    {
      p.copy_from(buffer + i * p.total_point_size);

      if (lasfilter && lasfilter->filter(&p)) continue;
      if (!s.contains(p.get_x(), p.get_y(), p.get_z())) continue;
      if (p.get_withheld_flag() != 0) continue;
      if (lastransform) lastransform->transform(&p);

      PointLAS pl(&p);
      pl.FID = i;
      res.push_back(std::move(pl));
    }
  }

  // We sort the query by distance to (x,y)
  std::sort(res.begin(), res.end(), [x,y,z](const PointXYZ& a, const PointXYZ& b)
  {
    double distA = (a.x - x)*(a.x - x) + (a.y - y)*(a.y - y) + (a.z - z)*(a.z - z);
    double distB = (b.x - x)*(b.x - x) + (b.y - y)*(b.y - y) + (b.z - z)*(b.z - z);
    return distA < distB;
  });

  // We keep the k first results into the result
  if ((size_t)k < res.size()) res.resize(k);

  return true;
}

// Thread safe
bool LAS::get_point(size_t pos, PointLAS& pt, LASfilter* const lasfilter, LAStransform* const lastransform) const
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

bool LAS::add_attribute(int data_type, const std::string& name, const std::string& description, double scale, double offset, bool mem_realloc)
{
  data_type--;
  bool has_scale = scale != 1;
  bool has_offset = offset != 0;
  //bool has_no_data = false;
  //bool has_min = false;
  //bool has_max = false;
  //double min = 0;
  //double max = 0;
  //double no_data = -99999.0;

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
    last_error = "LASlib internal error: add_attribute failed"; // # nocov
    return false; // # nocov
  }

  header->update_extra_bytes_vlr();
  header->point_data_record_length += attribute.get_size();

  if (mem_realloc)
    return realloc_point_and_buffer();
  else
    return true;
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
  int target = 0;

  if (pdf == 2 || pdf == 3 || pdf == 5 || pdf == 7 || pdf == 8 || pdf == 10)
    return true;

  if (pdf == 0)      target = 2;
  else if (pdf == 1) target = 3;
  else if (pdf == 4) target = 5;
  else if (pdf == 6) target = 7;
  else if (pdf == 9) target = 10;
  else
  {
    last_error = "Internal error: unsupported pdf"; // # nocov
    return false; // # nocov
  }
  header->point_data_format = target;
  header->point_data_record_length += 3*2; // 3 x 16 bits = 3 x 2 bytes

  if (target == 10)
    header->point_data_record_length += 2; // 2 bytes for NIR

  return realloc_point_and_buffer();
}

void LAS::clean_index()
{
  clean_query();
  if (index)
  {
    delete index;
    index = nullptr;
  }
}

void LAS::clean_query()
{
  current_interval = 0;
  shape = nullptr;
  inside = false;
  read_started = false;
  intervals_to_read.clear();
}

void LAS::reindex()
{
  clean_index();
  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, 2);
  while (read_point()) index->insert(point.get_x(), point.get_y());
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

bool LAS::alloc_buffer()
{
  if (buffer != NULL)
  {
    last_error = "buffer already allocated"; // # nocov
    return false; // # nocov
  }

  buffer = (unsigned char*)malloc(capacity);
  if (buffer == NULL)
  {
    // # nocov start
    if (errno == ENOMEM)
      last_error = "Memory allocation failed: Insufficient memory";
    else
      last_error = "Memory allocation failed: Unknown error";

    return false;
    // # nocov end
  }

  return true;
}

bool LAS::realloc_buffer()
{
  if (capacity == 0)
  {
    free(buffer);
    buffer = 0;
    return true;
  }

  unsigned char* tmp = (unsigned char*)realloc((void*)buffer, capacity);

  if (tmp == NULL)
  {
    // # nocov start
    free(buffer);
    buffer = 0;

    if (errno == ENOMEM)
      last_error = "Memory reallocation failed: Insufficient memory";
    else
      last_error = "Memory reallocation failed: Unknown error";

    return false;
    // # nocov end
  }

  buffer = tmp;
  return true;
}

bool LAS::realloc_point_and_buffer()
{
  LASpoint new_point;
  new_point.init(header, header->point_data_format, header->point_data_record_length, header);

  size_t new_capacity = npoints * new_point.total_point_size;
  if (new_capacity > capacity)
  {
    capacity = new_capacity;
    if (!realloc_buffer()) return false;
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

LAStransform* LAS::make_z_transformer(const std::string& use_attribute) const
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

int LAS::guess_point_data_format(bool has_gps, bool has_rgb, bool has_nir)
{
  std::vector<int> formats = {0,1,2,3,6,7,8};

  if (has_nir) // format 8 or 10
    return 8;

  if (has_gps) // format 1,3:10
  {
    auto end = std::remove(formats.begin(), formats.end(), 0);
    formats.erase(end, formats.end());
    end = std::remove(formats.begin(), formats.end(), 2);
    formats.erase(end, formats.end());
  }

  if (has_rgb)  // format 3, 5, 7, 8
  {
    auto end = std::remove(formats.begin(), formats.end(), 0);
    formats.erase(end, formats.end());
    end = std::remove(formats.begin(), formats.end(), 1);
    formats.erase(end, formats.end());
    end = std::remove(formats.begin(), formats.end(), 6);
    formats.erase(end, formats.end());
  }

  return formats[0];
}

int LAS::get_header_size(int minor_version)
{
  int header_size = 0;

  switch (minor_version)
  {
  case 0:
  case 1:
  case 2:
    header_size = 227;
    break;
  case 3:
    header_size = 235;
    break;
  case 4:
    header_size = 375;
    break;
  default:
    header_size = -1;
  break;
  }

  return header_size;
}

int LAS::get_point_data_record_length(int point_data_format, int num_extrabytes)
{
  switch (point_data_format)
  {
  case 0: return 20 + num_extrabytes; break;
  case 1: return 28 + num_extrabytes; break;
  case 2: return 26 + num_extrabytes; break;
  case 3: return 34 + num_extrabytes; break;
  case 4: return 57 + num_extrabytes; break;
  case 5: return 63 + num_extrabytes; break;
  case 6: return 30 + num_extrabytes; break;
  case 7: return 36 + num_extrabytes; break;
  case 8: return 38 + num_extrabytes; break;
  case 9: return 59 + num_extrabytes; break;
  case 10: return 67 + num_extrabytes; break;
  default: return 0; break;
  }
}

U64 LAS::get_true_number_of_points() const
{
  return MAX(header->number_of_point_records, header->extended_number_of_point_records);
}

double LAS::get_x(const unsigned char* buf) const { unsigned int X = *((unsigned int*)buf);     return point.quantizer->get_x(X); };
double LAS::get_y(const unsigned char* buf) const { unsigned int Y = *((unsigned int*)(buf+4)); return point.quantizer->get_y(Y); };
double LAS::get_z(const unsigned char* buf) const { unsigned int Z = *((unsigned int*)(buf+8)); return point.quantizer->get_z(Z); };
double LAS::get_gpstime(const unsigned char* buf) const { if (point.is_extended_point_type()) return *((const double*)&buf[22]); else return *((const double*)&buf[20]); };
unsigned char LAS::get_scanner_channel(const unsigned char* buf) const { if (point.is_extended_point_type()) return (buf[15] >> 4) & 0x03; else return 0; };
unsigned char LAS::get_return_number(const unsigned char* buf) const { return buf[14] & 0x0F; };

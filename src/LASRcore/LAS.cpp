#include "LAS.h"
#include "GridPartition.h"
#include "Raster.h"
#include "macros.h"
#include "error.h"

#include "lasdefinitions.hpp"
#include "lasfilter.hpp"
#include "lasutility.hpp"
#include "lasreader.hpp"

#include <algorithm>

LAS::LAS(Header* newheader)
{
  this->newheader = newheader;

  // Point cloud storage
  buffer = NULL;
  npoints = 0;
  capacity = 0;

  current_point = 0;
  next_point = 0;
  read_started = false;

  // For spatial indexing
  index = new GridPartition(newheader->min_x, newheader->min_y, newheader->max_x, newheader->max_y, 2);
  current_interval = 0;
  shape = nullptr;
  inside = false;

  // Initialize the good point format
  p.set_schema(&newheader->schema);
}

LAS::LAS(const Raster& raster)
{
  // Point cloud storage
  buffer = NULL;
  npoints = 0;
  capacity = 0;

  current_point = 0;
  next_point = 0;
  read_started = false;

  // Convert the raster to a LAS
  newheader = new Header;
  /*newheader->file_source_ID       = 0;
  newheader->version_major        = 1;
  newheader->version_minor        = 2;
  newheader->header_size          = 227;
  newheader->offset_to_point_data = 227;
  newheader->file_creation_year   = 0;
  newheader->file_creation_day    = 0;
  newheader->point_data_format    = 0;
  newheader->x_scale_factor       = 0.01;
  newheader->y_scale_factor       = 0.01;
  newheader->z_scale_factor       = 0.01;
  newheader->x_offset             = raster.get_full_extent()[0];
  newheader->y_offset             = raster.get_full_extent()[1];
  newheader->z_offset             = 0;*/
  newheader->number_of_point_records = raster.get_ncells();
  newheader->min_x                = raster.get_xmin()-raster.get_xres()/2;
  newheader->min_y                = raster.get_ymin()-raster.get_yres()/2;
  newheader->max_x                = raster.get_xmax()+raster.get_xres()/2;
  newheader->max_y                = raster.get_ymax()-raster.get_yres()/2;
  newheader->schema.add_attribute("X", AttributeType::INT32, 0.001, raster.get_full_extent()[0]);
  newheader->schema.add_attribute("Y", AttributeType::INT32, 0.001, raster.get_full_extent()[1]);
  newheader->schema.add_attribute("Z", AttributeType::INT32, 0.001, 0);
  newheader->schema.add_attribute("Flags", AttributeType::INT8);

  // For spatial indexing
  current_interval = 0;
  shape = nullptr;
  inside = false;
  index = new GridPartition(newheader->min_x, newheader->min_y, newheader->max_x, newheader->max_y, raster.get_xres()*4);

  p = Point(&newheader->schema);

  for (int i = 0 ; i < raster.get_ncells() ; i++)
  {
    float z = raster.get_value(i);

    if (raster.is_na(z)) continue;

    double x = raster.x_from_cell(i);
    double y = raster.y_from_cell(i);

    p.set_x(x);
    p.set_y(y);
    p.set_z(z);

    add_point(p);
  }

  newheader->number_of_point_records = npoints;
}

LAS::~LAS()
{
  if (newheader)
  {
    delete newheader;
    newheader = nullptr;
  }

  if (buffer)
  {
    free(buffer);
    buffer = NULL;
  }

  clean_index();
}

bool LAS::add_point(const Point& p)
{
  if (buffer == NULL)
  {
    capacity = 100000 * newheader->schema.total_point_size;
    if (!alloc_buffer()) return false;
  }

  if (npoints == I32_MAX)
  {
    last_error = "LASR cannot stores more than 2147483647 points"; // # nocov
    return false;                                                  // # nocov
  }

  // Realloc memory and increase buffer size if needed
  size_t required_capacity = npoints*newheader->schema.total_point_size;
  if (required_capacity == capacity)
  {
    size_t capacity_max = get_true_number_of_points()*newheader->schema.total_point_size;

    // This may happens if the newheader is not properly populated
    if (required_capacity >= capacity_max)
      capacity_max = capacity*2; // # nocov

    if (capacity_max < capacity*2)
      capacity = capacity_max;
    else
      capacity *= 2;

    if (!realloc_buffer()) return false;
  }

  memcpy(buffer + npoints * newheader->schema.total_point_size, p.data, newheader->schema.total_point_size);
  npoints++;

  index->insert(p.get_x(), p.get_y());

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
  p.data = buffer + current_point * newheader->schema.total_point_size;
  return true;
}

// Thread safe and fast
/*bool LAS::get_xyz(size_t pos, double* xyz) const
{
  if (pos >= npoints)
  {
    last_error = "seek out of bounds"; // # nocov
    return false; // # nocov
  }

  unsigned char* buf = buffer + pos * newheader->schema.total_point_size;
  xyz[0] = get_x(buf);
  xyz[1] = get_y(buf);
  xyz[2] = get_z(buf);
  return true;
}*/

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
    p.data = buffer + current_point * newheader->schema.total_point_size;
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
      if (shape->contains(p.get_x(), p.get_y()))
      {
        if (include_withhelded || !p.get_deleted()) return true;
      }
    }
    else
    {
      if (include_withhelded || !p.get_deleted()) return true;
    }
  } while (true);

  return true;
}

void LAS::update_point()
{
  //point.copy_to(buffer + current_point * newheader->schema.total_point_size);
}

void LAS::remove_point()
{
  /*point.set_withheld_flag(1);
  update_point();*/
}

void LAS::delete_point(Point* p)
{
  if (p == nullptr)
  {
    this->p.set_deleted();
    newheader->number_of_point_records--;
  }
  else
  {
    p->set_deleted();
    if (!p->own_data) newheader->number_of_point_records--;
  }
}

bool LAS::delete_deleted()
{
  clean_index();
  index = new GridPartition(newheader->min_x, newheader->min_y, newheader->max_x, newheader->max_y, 2);

  int j = 0;
  for (int i = 0 ; i < npoints ; i++)
  {
    seek(i);
    if (p.get_deleted())
    {
      p.data = buffer + j * newheader->schema.total_point_size;
      index->insert(p.get_x(), p.get_y());
      j++;
    }
  }

  double ratio = (double)j/(double)npoints;
  npoints = j;

  if (ratio < 0.5)
  {
    capacity = npoints*newheader->schema.total_point_size;
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
  const Attribute* gpstime = p.schema->find_attribute("gpstime");
  const Attribute* returnnumber = p.schema->find_attribute("ReturnNumber");
  bool have_gpstime = gpstime != nullptr;
  bool have_returnnumber = returnnumber != nullptr;

  return false;

  if (have_gpstime)
    qsort((void*)buffer, npoints, newheader->schema.total_point_size, compare_buffers);
  else
    qsort((void*)buffer, npoints, newheader->schema.total_point_size, compare_buffers_nogps);

  reindex();

  return true;
}

bool LAS::sort(const std::vector<int>& order)
{
  std::vector<bool> visited(npoints, false);
  char* temp = (char*)malloc(newheader->schema.total_point_size);
  size_t chunk_size = newheader->schema.total_point_size;

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
  /*LASinventory inventory;
  while (read_point()) inventory.add(&point);
  inventory.update_newheader(newheader);*/
}

// Thread safe
bool LAS::query(const Shape* const shape, std::vector<Point>& addr, PointFilter* const filter) const
{
  Point p;
  p.set_schema(&newheader->schema);

  addr.clear();

  std::vector<Interval> intervals;
  index->query(shape->xmin(), shape->ymin(), shape->xmax(), shape->ymax(), intervals);

  if (intervals.size() == 0) return false;

  for (const auto& interval : intervals)
  {
    for (int i = interval.start ; i <= interval.end ; i++)
    {
      p.data = buffer + i * newheader->schema.total_point_size;

      if (filter && filter->filter(&p)) continue;

      if (!p.get_deleted() && shape->contains(p.get_x(), p.get_y()))
      {
         addr.push_back(p);
      }
    }
  }

  return addr.size() > 0;
}

bool LAS::query(const std::vector<Interval>& intervals, std::vector<Point>& addr, PointFilter* const filter) const
{
  Point p;
  p.set_schema(&newheader->schema);

  addr.clear();

  if (intervals.size() == 0) return false;

  for (const auto& interval : intervals)
  {
    for (int i = interval.start ; i <= interval.end ; i++)
    {
      p.data = buffer + i * newheader->schema.total_point_size;

      if (filter && filter->filter(&p)) continue;

      if (!p.get_deleted())
      {
         addr.push_back(p);
      }
    }
  }

  return addr.size() > 0;
}

// Thread safe
bool LAS::knn(const Point& xyz, int k, double radius_max, std::vector<Point>& res, PointFilter* const filter) const
{
  double x = xyz.get_x();
  double y = xyz.get_y();
  double z = xyz.get_z();

  Point p;
  p.set_schema(&newheader->schema);

  double area = (newheader->max_x-newheader->min_x)*(newheader->max_y-newheader->min_y);
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
            p.data = buffer + i * newheader->schema.total_point_size;
            if (p.get_deleted()) continue;
            //if (lasfilter && lasfilter->filter(&p)) continue;
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
      p.data = buffer + i * newheader->schema.total_point_size;

      //if (lasfilter && lasfilter->filter(&p)) continue;
      if (!s.contains(p.get_x(), p.get_y(), p.get_z())) continue;
      if (p.get_deleted()) continue;

      res.push_back(p);
    }
  }

  // We sort the query by distance to (x,y)
  std::sort(res.begin(), res.end(), [x,y,z](const Point& a, const Point& b)
  {
    double distA = (a.get_x() - x)*(a.get_x() - x) + (a.get_y() - y)*(a.get_y() - y) + (a.get_z() - z)*(a.get_z() - z);
    double distB = (b.get_x() - x)*(b.get_x() - x) + (b.get_y() - y)*(b.get_y() - y) + (b.get_z() - z)*(b.get_z() - z);
    return distA < distB;
  });

  // We keep the k first results into the result
  if ((size_t)k < res.size()) res.resize(k);

  return true;
}

bool LAS::get_point(size_t pos, Point* p, PointFilter* const filter) const
{
  p->data = buffer + pos * newheader->schema.total_point_size;
  if (p->get_deleted()) return false;
  if (filter && filter->filter(p)) return false;
  //pt.copy(&p);
  //if (accessor) pt.z = (*accessor)(&p);
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

bool LAS::add_attribute(const Attribute& attribute)
{
  size_t previous_size = newheader->schema.total_point_size;
  size_t new_size = previous_size + attribute.size;
  size_t new_capacity = get_true_number_of_points() * new_size;

  newheader->schema.add_attribute(attribute);

  if (new_capacity > capacity)
  {
    capacity = new_capacity;
    if (!realloc_buffer()) return false;
  }

  for (int i = get_true_number_of_points()-1 ; i >= 0 ; --i)
  {
    memcpy(buffer + i * new_size, buffer + i * previous_size, previous_size);
  }

  return true;
}

bool LAS::add_attributes(const std::vector<Attribute>& attributes)
{
  size_t previous_size = newheader->schema.total_point_size;
  size_t new_size = previous_size;

  for (const auto& attribute : attributes)
  {
    new_size += attribute.size;
    newheader->schema.add_attribute(attribute);
  }

  size_t new_capacity = get_true_number_of_points() * new_size;

  if (new_capacity > capacity)
  {
    capacity = new_capacity;
    if (!realloc_buffer()) return false;
  }

  for (int i = get_true_number_of_points()-1 ; i >= 0 ; --i)
  {
    memcpy(buffer + i * new_size, buffer + i * previous_size, previous_size);
  }

  return true;
}

/*void LAS::set_index(bool index)
{
  clean_index();
  index = new GridPartition(lasnewheader.min_x, lasnewheader.min_y, lasnewheader.max_x, lasnewheader.max_y, 10);
  while (read_point()) index->insert(point);
}

void LAS::set_index(float res)
{
  clean_index();
  index = new GridPartition(lasnewheader.min_x, lasnewheader.min_y, lasnewheader.max_x, lasnewheader.max_y, res);
  while (read_point()) index->insert(point);
}*/


bool LAS::add_rgb()
{
  Attribute R("R", AttributeType::INT16, 0, 0, "Red channel");
  Attribute G("G", AttributeType::INT16, 0, 0, "Green channel");
  Attribute B("B", AttributeType::INT16, 0, 0, "Blue channe");
  add_attributes({R,G,B});
  return true;
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
  index = new GridPartition(newheader->min_x, newheader->min_y, newheader->max_x, newheader->max_y, 2);
  while (read_point()) index->insert(p.get_x(), p.get_y());
}

bool LAS::is_attribute_loadable(int index)
{
  if (index < 0) return false;
  //if (newheader->number_attributes-1 < index) return false;

  AttributeType data_type = p.schema->attributes[index].type;

  if (data_type == AttributeType::INT64 || data_type == AttributeType::UINT64)
  {
    warning("unsigned 32 bits integers and 64 bits integers are not supported in R");
    return false;
  }

  return false; // to trigger an error
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

/*bool LAS::realloc_point_and_buffer()
{
  size_t new_capacity = npoints * newheader->schema.total_point_size;
  if (new_capacity > capacity)
  {
    capacity = new_capacity;
    if (!realloc_buffer()) return false;
  }

  for (int i = npoints-1 ; i >= 0 ; --i)
  {
    seek(i);
    new_point = point;
    new_point.copy_to(buffer + i * new_newheader->schema.total_point_size);
  }

  point = LASpoint();
  point.init(newheader, newheader->point_data_format, newheader->point_data_record_length, newheader);

  return false;
}*/

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
  return newheader->number_of_point_records;
}

/*double LAS::get_x(int i) const { unsigned int X = *(buffer + i * newheader->schema.total_point_size + 0); return point.quantizer->get_x(X); };
double LAS::get_y(int i) const { unsigned int Y = *(buffer + i * newheader->schema.total_point_size + 4); return point.quantizer->get_y(Y); };
double LAS::get_z(int i) const { unsigned int Z = *(buffer + i * newheader->schema.total_point_size + 8); return point.quantizer->get_z(Z); };

double LAS::get_x(const unsigned char* buf) const { unsigned int X = *((unsigned int*)buf);     return point.quantizer->get_x(X); };
double LAS::get_y(const unsigned char* buf) const { unsigned int Y = *((unsigned int*)(buf+4)); return point.quantizer->get_y(Y); };
double LAS::get_z(const unsigned char* buf) const { unsigned int Z = *((unsigned int*)(buf+8)); return point.quantizer->get_z(Z); };
double LAS::get_gpstime(const unsigned char* buf) const { if (point.is_extended_point_type()) return *((const double*)&buf[22]); else return *((const double*)&buf[20]); };
unsigned char LAS::get_scanner_channel(const unsigned char* buf) const { if (point.is_extended_point_type()) return (buf[15] >> 4) & 0x03; else return 0; };
unsigned char LAS::get_return_number(const unsigned char* buf) const { return buf[14] & 0x0F; };
*/
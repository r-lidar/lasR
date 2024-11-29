#include "PointCloud.h"
#include "GridPartition.h"
#include "Raster.h"
#include "macros.h"
#include "error.h"

#include <algorithm>

PointCloud::PointCloud(Header* header)
{
  this->header = header;

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
  point.set_schema(&header->schema);
}

PointCloud::PointCloud(const Raster& raster)
{
  // Point cloud storage
  buffer = NULL;
  npoints = 0;
  capacity = 0;

  current_point = 0;
  next_point = 0;
  read_started = false;

  // Convert the raster to a PointCloud
  header = new Header;
  /*header->file_source_ID       = 0;
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
  header->z_offset             = 0;*/
  header->number_of_point_records = raster.get_ncells();
  header->min_x                = raster.get_xmin()-raster.get_xres()/2;
  header->min_y                = raster.get_ymin()-raster.get_yres()/2;
  header->max_x                = raster.get_xmax()+raster.get_xres()/2;
  header->max_y                = raster.get_ymax()-raster.get_yres()/2;
  header->schema.add_attribute("X", AttributeType::INT32, 0.001, raster.get_full_extent()[0]);
  header->schema.add_attribute("Y", AttributeType::INT32, 0.001, raster.get_full_extent()[1]);
  header->schema.add_attribute("Z", AttributeType::INT32, 0.001, 0);
  header->schema.add_attribute("Flags", AttributeType::INT8);

  // For spatial indexing
  current_interval = 0;
  shape = nullptr;
  inside = false;
  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, raster.get_xres()*4);

  point = Point(&header->schema);

  for (int i = 0 ; i < raster.get_ncells() ; i++)
  {
    float z = raster.get_value(i);

    if (raster.is_na(z)) continue;

    double x = raster.x_from_cell(i);
    double y = raster.y_from_cell(i);

    point.set_x(x);
    point.set_y(y);
    point.set_z(z);

    add_point(point);
  }

  header->number_of_point_records = npoints;
}

PointCloud::~PointCloud()
{
  if (header)
  {
    delete header;
    header = nullptr;
  }

  if (buffer)
  {
    free(buffer);
    buffer = NULL;
  }

  clean_index();
}

bool PointCloud::add_point(const Point& p)
{
  if (buffer == NULL)
  {
    capacity = 100000 * header->schema.total_point_size;
    if (!alloc_buffer()) return false;
  }

  if (npoints == std::numeric_limits<int>::max())
  {
    last_error = "LASR cannot stores more than 2147483647 points"; // # nocov
    return false;                                                  // # nocov
  }

  // Realloc memory and increase buffer size if needed
  size_t required_capacity = npoints*header->schema.total_point_size;
  if (required_capacity == capacity)
  {
    size_t capacity_max = get_true_number_of_points()*header->schema.total_point_size;

    // This may happens if the header is not properly populated
    if (required_capacity >= capacity_max)
      capacity_max = capacity*2; // # nocov

    if (capacity_max < capacity*2)
      capacity = capacity_max;
    else
      capacity *= 2;

    if (!realloc_buffer()) return false;
  }

  memcpy(buffer + npoints * header->schema.total_point_size, p.data, header->schema.total_point_size);
  npoints++;

  index->insert(p.get_x(), p.get_y());

  return true;
}

bool PointCloud::seek(size_t pos)
{
  if (pos >= npoints)
  {
    last_error = "seek out of bounds"; // # nocov
    return false; // # nocov
  }

  current_point = pos;
  next_point = pos;
  next_point++;
  point.data = buffer + current_point * header->schema.total_point_size;
  return true;
}

// Thread safe and fast
/*bool PointCloud::get_xyz(size_t pos, double* xyz) const
{
  if (pos >= npoints)
  {
    last_error = "seek out of bounds"; // # nocov
    return false; // # nocov
  }

  unsigned char* buf = buffer + pos * header->schema.total_point_size;
  xyz[0] = get_x(buf);
  xyz[1] = get_y(buf);
  xyz[2] = get_z(buf);
  return true;
}*/

bool PointCloud::read_point(bool include_withhelded)
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
    point.data = buffer + current_point * header->schema.total_point_size;
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
        if (include_withhelded || !point.get_deleted()) return true;
      }
    }
    else
    {
      if (include_withhelded || !point.get_deleted()) return true;
    }
  } while (true);

  return true;
}

void PointCloud::update_point()
{
  //point.copy_to(buffer + current_point * header->schema.total_point_size);
}

void PointCloud::remove_point()
{
  /*point.set_withheld_flag(1);
  update_point();*/
}

void PointCloud::delete_point(Point* p)
{
  if (p == nullptr)
  {
    this->point.set_deleted();
    header->number_of_point_records--;
  }
  else
  {
    p->set_deleted();
    if (!p->own_data) header->number_of_point_records--;
  }
}

bool PointCloud::delete_deleted()
{
  clean_index();
  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, 2);

  int j = 0;
  for (int i = 0 ; i < npoints ; i++)
  {
    seek(i);
    if (point.get_deleted())
    {
      point.data = buffer + j * header->schema.total_point_size;
      index->insert(point.get_x(), point.get_y());
      j++;
    }
  }

  double ratio = (double)j/(double)npoints;
  npoints = j;

  if (ratio < 0.5)
  {
    capacity = npoints*header->schema.total_point_size;
    if (!realloc_buffer()) return false;
  }

  return true;
}


static inline double get_gps_time(const unsigned char* buf, size_t offset) { return *((const double*)&buf[offset]); }
static inline unsigned char get_return_number(const unsigned char* buf, size_t offset) { return buf[offset] ; }

static int compare_buffers(const void* a, const void* b, void* context)
{
  size_t* offsets = (size_t*)context;

  const unsigned char* buf_a = (const unsigned char*)a;
  const unsigned char* buf_b = (const unsigned char*)b;

  if (get_gps_time(buf_a, offsets[0]) < get_gps_time(buf_b, offsets[0])) return -1;
  if (get_gps_time(buf_a, offsets[0]) > get_gps_time(buf_b, offsets[0])) return 1;
  if (get_return_number(buf_a, offsets[1]) < get_return_number(buf_b, offsets[1])) return -1;
  if (get_return_number(buf_a, offsets[1]) > get_return_number(buf_b, offsets[1])) return 1;
  return 0;
}

static int compare_buffers_norn(const void* a, const void* b, void* context)
{
  size_t* offsets = (size_t*)context;
  const unsigned char* buf_a = (const unsigned char*)a;
  const unsigned char* buf_b = (const unsigned char*)b;

  if (get_gps_time(buf_a, offsets[0]) < get_gps_time(buf_b, offsets[0])) return -1;
  if (get_gps_time(buf_a, offsets[0]) > get_gps_time(buf_b, offsets[0])) return 1;
  return 0;
}

static int compare_buffers_nogps(const void* a, const void* b, void* context)
{
  size_t* offsets = (size_t*)context;

  const unsigned char* buf_a = (const unsigned char*)a;
  const unsigned char* buf_b = (const unsigned char*)b;

  if (get_return_number(buf_a, offsets[1]) < get_return_number(buf_b, offsets[1])) return -1;
  if (get_return_number(buf_a, offsets[1]) > get_return_number(buf_b, offsets[1])) return 1;
  return 0;
}

#include <stdlib.h>

/*bool PointCloud::sort()
{
  const Attribute* gpstime = point.schema->find_attribute("gpstime");
  const Attribute* returnnumber = point.schema->find_attribute("ReturnNumber");
  bool have_gpstime = gpstime != nullptr;
  bool have_returnnumber = returnnumber != nullptr;
  have_gpstime = false;

  size_t gpstime_offset = have_gpstime ? gpstime->offset : 0;
  size_t returnnumber_offset = have_returnnumber ? returnnumber->offset : 0;

  size_t offsets[2] = { gpstime_offset, returnnumber_offset };

  print("Offsets: %lu %lu\n", gpstime_offset, returnnumber_offset);

  if (have_gpstime && have_returnnumber)
    qsort_s((void*)buffer, npoints, header->schema.total_point_size, compare_buffers, offsets);
  else if (!have_gpstime && have_returnnumber)
    qsort_s((void*)buffer, npoints, header->schema.total_point_size, compare_buffers_nogps, offsets);
  else if (have_gpstime && !have_returnnumber)
    qsort_s((void*)buffer, npoints, header->schema.total_point_size, compare_buffers_norn, offsets);
  else
    return true;

  reindex();

  return true;
}*/


bool PointCloud::sort(const std::vector<int>& order)
{
  std::vector<bool> visited(npoints, false);
  char* temp = (char*)malloc(header->schema.total_point_size);
  size_t chunk_size = header->schema.total_point_size;

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

void PointCloud::update_header()
{
  /*LASinventory inventory;
  while (read_point()) inventory.add(&point);
  inventory.update_header(header);*/
}

// Thread safe
bool PointCloud::query(const Shape* const shape, std::vector<Point>& addr, PointFilter* const filter) const
{
  Point p;
  p.set_schema(&header->schema);

  addr.clear();

  std::vector<Interval> intervals;
  index->query(shape->xmin(), shape->ymin(), shape->xmax(), shape->ymax(), intervals);

  if (intervals.size() == 0) return false;

  for (const auto& interval : intervals)
  {
    for (int i = interval.start ; i <= interval.end ; i++)
    {
      p.data = buffer + i * header->schema.total_point_size;

      if (filter && filter->filter(&p)) continue;

      if (!p.get_deleted() && shape->contains(p.get_x(), p.get_y()))
      {
         addr.push_back(p);
      }
    }
  }

  return addr.size() > 0;
}

bool PointCloud::query(const std::vector<Interval>& intervals, std::vector<Point>& addr, PointFilter* const filter) const
{
  Point p;
  p.set_schema(&header->schema);

  addr.clear();

  if (intervals.size() == 0) return false;

  for (const auto& interval : intervals)
  {
    for (int i = interval.start ; i <= interval.end ; i++)
    {
      p.data = buffer + i * header->schema.total_point_size;

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
bool PointCloud::knn(const Point& xyz, int k, double radius_max, std::vector<Point>& res, PointFilter* const filter) const
{
  double x = xyz.get_x();
  double y = xyz.get_y();
  double z = xyz.get_z();

  Point p;
  p.set_schema(&header->schema);

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
            p.data = buffer + i * header->schema.total_point_size;
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
      p.data = buffer + i * header->schema.total_point_size;

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

bool PointCloud::get_point(size_t pos, Point* p, PointFilter* const filter) const
{
  p->data = buffer + pos * header->schema.total_point_size;
  if (p->get_deleted()) return false;
  if (filter && filter->filter(p)) return false;
  //pt.copy(&p);
  //if (accessor) pt.z = (*accessor)(&p);
  return true;
}

void PointCloud::set_inside(Shape* shape)
{
  clean_query();
  inside = true;
  this->shape = shape;
}

void PointCloud::set_intervals_to_read(const std::vector<Interval>& intervals)
{
  clean_query();
  inside = true;
  intervals_to_read = intervals;
}

bool PointCloud::add_attribute(const Attribute& attribute)
{
  size_t previous_size = header->schema.total_point_size;
  size_t new_size = previous_size + attribute.size;
  size_t new_capacity = get_true_number_of_points() * new_size;

  header->schema.add_attribute(attribute);

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

bool PointCloud::add_attributes(const std::vector<Attribute>& attributes)
{
  size_t previous_size = header->schema.total_point_size;
  size_t new_size = previous_size;

  for (const auto& attribute : attributes)
  {
    new_size += attribute.size;
    header->schema.add_attribute(attribute);
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

/*void PointCloud::set_index(bool index)
{
  clean_index();
  index = new GridPartition(lasheader.min_x, lasheader.min_y, lasheader.max_x, lasheader.max_y, 10);
  while (read_point()) index->insert(point);
}

void PointCloud::set_index(float res)
{
  clean_index();
  index = new GridPartition(lasheader.min_x, lasheader.min_y, lasheader.max_x, lasheader.max_y, res);
  while (read_point()) index->insert(point);
}*/


bool PointCloud::add_rgb()
{
  Attribute R("R", AttributeType::INT16, 0, 0, "Red channel");
  Attribute G("G", AttributeType::INT16, 0, 0, "Green channel");
  Attribute B("B", AttributeType::INT16, 0, 0, "Blue channe");
  add_attributes({R,G,B});
  return true;
}

void PointCloud::clean_index()
{
  clean_query();
  if (index)
  {
    delete index;
    index = nullptr;
  }
}

void PointCloud::clean_query()
{
  current_interval = 0;
  shape = nullptr;
  inside = false;
  read_started = false;
  intervals_to_read.clear();
}

void PointCloud::reindex()
{
  clean_index();
  index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, 2);
  while (read_point()) index->insert(point.get_x(), point.get_y());
}

bool PointCloud::is_attribute_loadable(int index)
{
  if (index < 0) return false;
  //if (header->number_attributes-1 < index) return false;

  AttributeType data_type = point.schema->attributes[index].type;

  if (data_type == AttributeType::INT64 || data_type == AttributeType::UINT64)
  {
    warning("unsigned 32 bits integers and 64 bits integers are not supported in R");
    return false;
  }

  return false; // to trigger an error
}

bool PointCloud::alloc_buffer()
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

bool PointCloud::realloc_buffer()
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

/*bool PointCloud::realloc_point_and_buffer()
{
  size_t new_capacity = npoints * header->schema.total_point_size;
  if (new_capacity > capacity)
  {
    capacity = new_capacity;
    if (!realloc_buffer()) return false;
  }

  for (int i = npoints-1 ; i >= 0 ; --i)
  {
    seek(i);
    new_point = point;
    new_point.copy_to(buffer + i * new_header->schema.total_point_size);
  }

  point = LASpoint();
  point.init(header, header->point_data_format, header->point_data_record_length, header);

  return false;
}*/

uint64_t PointCloud::get_true_number_of_points() const
{
  return header->number_of_point_records;
}

/*double PointCloud::get_x(int i) const { unsigned int X = *(buffer + i * header->schema.total_point_size + 0); return point.quantizer->get_x(X); };
double PointCloud::get_y(int i) const { unsigned int Y = *(buffer + i * header->schema.total_point_size + 4); return point.quantizer->get_y(Y); };
double PointCloud::get_z(int i) const { unsigned int Z = *(buffer + i * header->schema.total_point_size + 8); return point.quantizer->get_z(Z); };

double PointCloud::get_x(const unsigned char* buf) const { unsigned int X = *((unsigned int*)buf);     return point.quantizer->get_x(X); };
double PointCloud::get_y(const unsigned char* buf) const { unsigned int Y = *((unsigned int*)(buf+4)); return point.quantizer->get_y(Y); };
double PointCloud::get_z(const unsigned char* buf) const { unsigned int Z = *((unsigned int*)(buf+8)); return point.quantizer->get_z(Z); };
double PointCloud::get_gpstime(const unsigned char* buf) const { if (point.is_extended_point_type()) return *((const double*)&buf[22]); else return *((const double*)&buf[20]); };
unsigned char PointCloud::get_scanner_channel(const unsigned char* buf) const { if (point.is_extended_point_type()) return (buf[15] >> 4) & 0x03; else return 0; };
unsigned char PointCloud::get_return_number(const unsigned char* buf) const { return buf[14] & 0x0F; };
*/
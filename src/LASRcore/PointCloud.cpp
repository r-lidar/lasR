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
  index = nullptr;
  kdtree = nullptr;
  current_interval = 0;
  shape = nullptr;
  inside = false;

  // Initialize the good point format
  point.set_schema(&header->schema);
}

#ifndef NOGDAL
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
  header->signature = "From Raster";
  header->x_scale_factor       = 0.001;
  header->y_scale_factor       = 0.001;
  header->z_scale_factor       = 0.001;
  header->x_offset             = (int)raster.get_full_extent()[0];
  header->y_offset             = (int)raster.get_full_extent()[1];
  header->z_offset             = 0;
  header->number_of_point_records = raster.get_ncells();
  header->min_x                = raster.get_xmin()-raster.get_xres()/2;
  header->min_y                = raster.get_ymin()-raster.get_yres()/2;
  header->max_x                = raster.get_xmax()+raster.get_xres()/2;
  header->max_y                = raster.get_ymax()-raster.get_yres()/2;
  header->schema.add_attribute("Flags", AttributeType::INT8);
  header->schema.add_attribute("X", AttributeType::INT32, header->x_scale_factor, header->x_offset);
  header->schema.add_attribute("Y", AttributeType::INT32, header->y_scale_factor, header->y_offset);
  header->schema.add_attribute("Z", AttributeType::INT32, header->z_scale_factor, header->z_offset);

  // For spatial indexing
  current_interval = 0;
  shape = nullptr;
  inside = false;
  index = nullptr;
  kdtree = nullptr;
  Point p(&header->schema);
  point.set_schema(&header->schema);

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

  header->number_of_point_records = npoints;

  build_spatialindex();
}
#endif

PointCloud::~PointCloud()
{
  /*print("Destructor\n===========\n");
  header->schema.dump();
  print("npoints %lu\n", npoints);
  print("Buffer range:\n");
  print("%p %p\n", buffer, buffer + npoints*header->schema.total_point_size);
  print("%p %p\n", buffer, buffer  + capacity);
  print("Point adress %p\n", point.data);
  print("Point belong in the buffer?: %s\n",(point.data >= buffer && point.data < buffer + npoints*header->schema.total_point_size)?"true":"false");
  print("Point belong in the data?: %s\n", (point.data >= buffer && point.data < buffer + capacity)?"true":"false");
  print("Distance %lu\n", std::distance(buffer, point.data));*/
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

  clean_spatialindex();
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

  //index->insert(p.get_x(), p.get_y());

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
  // ratio = actual number of non deleted points (assuming the header is up to date with data)
  // divided by the actually number of points including the ones flagged as deleted.
  double ratio = (double)header->number_of_point_records/(double)npoints;

  // If more than 25% of deleted point then we reshaped the memory layout to free some memory
  // and reshape the spatial index. Otherwise no need to spend time on this task
  if (ratio > 0.75) return true;

  // Read all the points and move memory at the beginning of the buffer.
  int j = 0;
  for (int i = 0 ; i < npoints ; i++)
  {
    seek(i);
    if (!point.get_deleted())
    {
      memcpy(buffer + j * header->schema.total_point_size, point.data, header->schema.total_point_size);
      j++;
    }
  }
  npoints = j;

  // Rebuild the spatial index;
  build_spatialindex();

  // We move the point in the buffer, but the memory is still allocated. We recompute the capacity
  // and realloc the memory for this new capacity.
  capacity = npoints*header->schema.total_point_size;
  return realloc_buffer();
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

  build_spatialindex();

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

  build_spatialindex();

  return true;
}

void PointCloud::update_header()
{
  header->number_of_point_records = 0;
  header->min_x = std::numeric_limits<double>::max();
  header->min_y = std::numeric_limits<double>::max();
  header->min_z = std::numeric_limits<double>::max();
  header->max_x = std::numeric_limits<double>::lowest();
  header->max_y = std::numeric_limits<double>::lowest();
  header->max_z = std::numeric_limits<double>::lowest();

  double x, y, z;

  while (read_point())
  {
    x = point.get_x();
    y = point.get_y();
    z = point.get_z();

    // Update bounding box values
    if (x < header->min_x) header->min_x = x;
    if (y < header->min_y) header->min_y = y;
    if (z < header->min_z) header->min_z = z;

    if (x > header->max_x) header->max_x = x;
    if (y > header->max_y) header->max_y = y;
    if (z > header->max_z) header->max_z = z;

    header->number_of_point_records++;
  }
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
bool PointCloud::knn(const Point& xyz, int k, std::vector<Point>& res, PointFilter* const filter) const
{
  res.clear();
  double query_pt[3] = { xyz.get_x(), xyz.get_y(), xyz.get_z() };

  // Count the actual number of point queried.
  // kdtree will always find the k-nn but some of these points
  // may actually be filtered out or be flagged as deleted. The query
  // is thus not actually k points
  int n = 0;

  // While we do not have actually k points we search with bigger k until we do have the
  // initial k request
  int current_k = k;
  std::vector<KDTree::IndexType> indices(current_k);
  std::vector<KDTree::DistanceType> dists(current_k);
  while (n < k && n < get_true_number_of_points())
  {
    // Perform knn search (returns the number of valid neighbors found)
    size_t found = kdtree->knnSearch(query_pt, current_k, indices.data(), dists.data());

    Point p;
    p.set_schema(&header->schema);

    for (size_t i = 0; i < found; ++i)
    {
      size_t idx = indices[i];
      p.data = buffer + idx * header->schema.total_point_size;

      if (filter && filter->filter(&p)) continue;

      if (!p.get_deleted())
      {
        res.push_back(p);
        n++;
        if (n >= k) break;
      }
    }

    if (n < k)
    {
      current_k *= 2;
      indices.resize(current_k);
      indices.resize(current_k);
      res.clear();
    }
  }

  return true;
}

bool PointCloud::rknn(const Point& xyz, int k, double r, std::vector<Point>& res, PointFilter* const filter) const
{
  if (r <= 0.0) return false;

  res.clear();
  double query_pt[3] = { xyz.get_x(), xyz.get_y(), xyz.get_z() };

  std::vector<nanoflann::ResultItem<KDTree::IndexType, KDTree::DistanceType>> matches;
  size_t found = kdtree->radiusSearch(query_pt, r, matches);

  Point p;
  p.set_schema(&header->schema);

  // Sort by distance
  std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) { return a.second < b.second; });

  size_t count = 0;
  for (const auto& match : matches)
  {
    if (count >= (size_t)k) break;

    size_t idx = match.first;
    p.data = buffer + idx * header->schema.total_point_size;

    if (filter && filter->filter(&p)) continue;
    if (p.get_deleted()) continue;

    res.push_back(p);
    count++;
  }

  return true;
}

bool PointCloud::query_sphere(const Point& xyz, double r, std::vector<Point>& res, PointFilter* const filter) const
{
  if (r <= 0.0)  return false;

  res.clear();
  double query_pt[3] = { xyz.get_x(), xyz.get_y(), xyz.get_z() };

  std::vector<nanoflann::ResultItem<KDTree::IndexType, KDTree::DistanceType>> matches;
  size_t found = kdtree->radiusSearch(query_pt, r, matches);

  Point p;
  p.set_schema(&header->schema);

  for (const auto& match : matches)
  {
    size_t idx = match.first;
    p.data = buffer + idx * header->schema.total_point_size;

    if (filter && filter->filter(&p)) continue;
    if (p.get_deleted()) continue;

    res.push_back(p);
  }

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
  // Check if this attribute already exist to avoid adding twice the same attribute
  const Attribute* attr = header->schema.find_attribute(attribute.name);
  if (attr)
  {
    if (*attr != attribute)
    {
      last_error = "Cannot add a second attribute '" +  attribute.name + "'";
      return false;
    }
  }
  // The attribute does not exist. Reallocate memory.
  else
  {
    size_t previous_size = header->schema.total_point_size;
    size_t new_size = previous_size + attribute.size;
    size_t new_capacity = npoints * new_size;

    header->schema.add_attribute(attribute);

    if (new_capacity > capacity)
    {
      capacity = new_capacity;
      if (!realloc_buffer()) return false;
    }

    for (int i = npoints-1 ; i >= 0 ; --i)
    {
      memcpy(buffer + i * new_size, buffer + i * previous_size, previous_size);
      memset(buffer + i * new_size + previous_size, 0, attribute.size); // zero the new data
    }

    // We need to reset the underlying memory of the
    // nanoflann adaptor
    adaptor.data = buffer;
    adaptor.schema = &header->schema;
  }
  return true;
}

bool PointCloud::remove_attribute(const std::string& name)
{
  // Find attribute
  const Attribute* attr = header->schema.find_attribute(name);
  if (!attr) return true; // nothing to remove

  size_t offset = attr->offset;
  size_t size_to_remove = attr->size;
  size_t old_point_size = header->schema.total_point_size;
  size_t new_point_size = old_point_size - size_to_remove;
  size_t tail_size = old_point_size - (offset + size_to_remove);

  // For each point from last to first:
  // We copy head bytes before attribute, then tail bytes after attribute,
  // packed together tightly in new_point_size bytes.
  for (size_t i = 0; i < npoints ; ++i)
  {
    unsigned char* src = buffer + i * old_point_size;
    unsigned char* dst = buffer + i * new_point_size;

    // Copy head bytes (before removed attribute)
    if (offset > 0) memmove(dst, src, offset);

    // Copy tail bytes (after removed attribute)
    if (tail_size > 0) memmove(dst + offset, src + offset + size_to_remove, tail_size);
  }

  // Update schema
  header->schema.remove_attribute(name);

  return true;
}


bool PointCloud::add_attributes(const std::vector<Attribute>& attributes)
{
  size_t previous_size = header->schema.total_point_size;
  size_t added_bytes = 0;

  for (const Attribute& attribute : attributes)
  {
    // Check if this attribute already exist to avoid adding twice the same attribute
    const Attribute* attr = header->schema.find_attribute(attribute.name);

    // The attribute exist. Is the new one the same? In this case we do not add an attribute
    // but we overwrite the previous one. Otherwise we fail.
    if (attr)
    {
      if (*attr != attribute)
      {
        last_error = "Cannot add a second attribute '" +  attribute.name + "'";
        return false;
      }
    }
    else
    {
      added_bytes += attribute.size;
      header->schema.add_attribute(attribute);
    }
  }

  if (added_bytes > 0)
  {
    size_t new_size = previous_size + added_bytes;
    size_t new_capacity = npoints * new_size;

    if (new_capacity > capacity)
    {
      capacity = new_capacity;
      if (!realloc_buffer()) return false;
    }

    for (int i = npoints-1 ; i >= 0 ; --i)
    {
      memcpy(buffer + i * new_size, buffer + i * previous_size, previous_size);
      memset(buffer + i * new_size + previous_size, 0, added_bytes); // zero the new data
    }
  }

  // We need to reset the underlying memory of the
  // nanoflann adaptor
  adaptor.data = buffer;
  adaptor.schema = &header->schema;

  return true;
}

/*void PointCloud::set_index(bool index)
{
  clean_spatialindex();
  index = new GridPartition(lasheader.min_x, lasheader.min_y, lasheader.max_x, lasheader.max_y, 10);
  while (read_point()) index->insert(point);
}

void PointCloud::set_index(float res)
{
  clean_spatialindex();
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

void PointCloud::build_spatialindex()
{
  clean_spatialindex();
  if (npoints > 0)
  {
    double res = GridPartition::guess_resolution_from_density(header->density());
    index = new GridPartition(header->min_x, header->min_y, header->max_x, header->max_y, res);
    while (read_point()) index->insert(point.get_x(), point.get_y());

    adaptor = PointCloudAdaptor(buffer, npoints, &header->schema);
    kdtree = new KDTree(3, adaptor, nanoflann::KDTreeSingleIndexAdaptorParams(10));
    kdtree->buildIndex();
  }
  else
  {
    index = new GridPartition(0, 0, 0, 0, 1);
  }
}

void PointCloud::clean_spatialindex()
{
  clean_query();
  if (index)
  {
    delete index;
    index = nullptr;
  }

  if (kdtree)
  {
    delete kdtree;
    kdtree = nullptr;
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

  buffer = (unsigned char*)calloc(capacity, sizeof(unsigned char));
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
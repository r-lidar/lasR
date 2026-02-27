#ifndef LASF_H
#define LASF_H

#include "Interval.h"
#include "Shape.h"
#include "PointSchema.h"
#include "PointFilter.h"
#include "Header.h"

#ifdef PI
#undef PI
#endif
#include "nanoflann/nanoflann.h"

#include <vector>
#include <string>

class GridPartition;
class Raster;

struct PointCloudAdaptor
{
  const unsigned char* data;
  size_t npoints;
  const AttributeSchema* schema;
  PointCloudAdaptor() = default;
  PointCloudAdaptor(const unsigned char* data, size_t npoints, const AttributeSchema* schema)
  {
    this->data = data;
    this->npoints = npoints;
    this->schema = schema;
  }

  inline size_t kdtree_get_point_count() const { return npoints; }
  inline double kdtree_get_pt(const size_t idx, int dim) const
  {
    const unsigned char* ptr = data + idx * schema->total_point_size;
    const auto& attr = schema->attributes[dim+1];
    switch(attr.type)
    {
      case INT32:{ int value = *((int*)(ptr + attr.offset)); return attr.scale_factor * value + attr.value_offset; }
      case FLOAT: { return *((float*)(ptr + attr.offset)); }
      case DOUBLE: { return *((double*)(ptr + attr.offset)); }
      default: return 0;
    }
  }

  template<class BBOX>
  bool kdtree_get_bbox(BBOX& bb) const { return false; }
};

using KDTree = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<double, PointCloudAdaptor>, PointCloudAdaptor, 3>;

class PointCloud
{
public:
  PointCloud(Header* header);
  ~PointCloud();
  bool add_attribute(const Attribute&);
  bool add_attributes(const std::vector<Attribute>&);
  bool add_point(const Point& p);
  bool add_rgb();
  bool remove_rgb();
  bool remove_attribute(const std::string&);
  bool remove_attributes(const std::vector<std::string>&);
  bool keep_attributes(const std::vector<std::string>&);
  bool seek(size_t pos);
  bool read_point(bool include_withhelded = false);
  void set_file(const std::string& file) { this->file = file; };
  void update_header();
  bool is_attribute_loadable(int index);
  void delete_point(Point* p = nullptr);
  bool delete_deleted();
  //bool sort();
  bool sort(const std::vector<int>& order);

  // Thread safe queries
  bool build_kdtree();
  bool build_partition();
  bool get_point(size_t pos, Point* p, PointFilter* const filter = nullptr) const;
  bool query(const Shape* const shape, std::vector<Point>& addr, PointFilter* const filter = nullptr) const;
  bool query(const std::vector<Interval>& intervals, std::vector<Point>& addr, PointFilter* const filter = nullptr) const;
  bool query_sphere(const Point& xyz, double r, std::vector<Point>& res, PointFilter* const filter) const;
  bool knn(const Point& xyz, int k, std::vector<Point>& res, PointFilter* const filter = nullptr) const;
  bool rknn(const Point& xyz, int k, double r, std::vector<Point>& res, PointFilter* const filter = nullptr) const;
  int get_index(Point* p) { size_t index = (size_t)(p->data - buffer); return(index/header->schema.total_point_size); }

  // Spatial queries
  void set_inside(Shape* shape);

  // Non spatial queries
  void set_intervals_to_read(const std::vector<Interval>& intervals);

  // Additionnal feature with GDAL
  #ifndef NOGDAL
  PointCloud(const Raster& raster);
  #endif

private:
  void clean_spatialindex();
  void clean_query();
  bool alloc_buffer();
  bool realloc_buffer();
  uint64_t get_true_number_of_points() const;

public:
  Header* header;
  Point point;
  size_t npoints;
  size_t current_point;

private:
  unsigned char* buffer;
  size_t capacity; // capacity of the buffer in bytes
  int next_point;

  // For spatial indexed search
  PointCloudAdaptor adaptor;
  GridPartition* gridpartition;
  KDTree* kdtree;
  int current_interval;
  std::vector<Interval> intervals_to_read;
  bool read_started;
  bool inside;
  Shape* shape;
  std::string file;

public:
  enum attributes{X, Y, Z, I, T, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, UD, SA, PSID, R, G, B, NIR, CHAN, BUFF};
};

#endif
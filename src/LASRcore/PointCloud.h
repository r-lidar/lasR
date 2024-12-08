#ifndef LASF_H
#define LASF_H

#include "Interval.h"
#include "Shape.h"
#include "PointSchema.h"
#include "PointFilter.h"
#include "Header.h"

#include <vector>
#include <string>

class GridPartition;
class Raster;
class LASfilter;
class LASheader;

class PointCloud
{
public:
  PointCloud(Header* header);
  PointCloud(const Raster& raster);
  ~PointCloud();
  bool add_attribute(const Attribute&);
  bool add_attributes(const std::vector<Attribute>&);
  bool add_point(const Point& p);
  bool add_rgb();
  bool seek(size_t pos);
  bool read_point(bool include_withhelded = false);
  void set_file(const std::string& file) { this->file = file; };
  void update_point();
  void remove_point();
  void update_header();
  bool is_indexed() { return index != 0; };
  bool is_attribute_loadable(int index);
  void delete_point(Point* p = nullptr);
  bool delete_deleted();
  //bool sort();
  bool sort(const std::vector<int>& order);

  // Thread safe queries
  bool get_point(size_t pos, Point* p, PointFilter* const filter = nullptr) const;
  bool query(const Shape* const shape, std::vector<Point>& addr, PointFilter* const filter = nullptr) const;
  bool query(const std::vector<Interval>& intervals, std::vector<Point>& addr, PointFilter* const filter = nullptr) const;
  bool knn(const Point& xyz, int k, double radius_max, std::vector<Point>& res, PointFilter* const filter = nullptr) const;

  int get_index(Point* p) { size_t index = (size_t)(p->data - buffer); return(index/header->schema.total_point_size); }

  // Spatial queries
  void set_inside(Shape* shape);

  // Non spatial queries
  void set_intervals_to_read(const std::vector<Interval>& intervals);

private:
  void clean_index();
  void reindex();
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
  GridPartition* index;
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
#ifndef LASF_H
#define LASF_H

#include "laszip.hpp"
#include "laspoint.hpp"
#include "lasdefinitions.hpp"

#include "Interval.h"
#include "Shape.h"
#include "PointLAS.h"
#include "Accessors.h"

#include <vector>
#include <string>

class GridPartition;
class Raster;
class LASfilter;

class LAS
{
public:
  LAS(LASheader* header);
  LAS(const Raster& raster);
  ~LAS();
  bool add_attribute(int data_type, const std::string& name, const std::string& description, double scale = 1, double offset = 0, bool mem_realloc = true);
  bool add_point(const LASpoint& p);
  bool add_rgb();
  bool seek(size_t pos);
  bool read_point(bool include_withhelded = false);
  void set_file(const std::string& file) { this->file = file; };
  void update_point();
  void remove_point();
  void update_header();
  bool is_indexed() { return index != 0; };
  bool is_attribute_loadable(int index);
  bool realloc_point_and_buffer();
  bool delete_withheld();
  bool sort();
  bool sort(const std::vector<int>& order);

  // Thread safe queries
  bool get_xyz(size_t pos, double* xyz) const;
  bool get_point(size_t pos, PointLAS& pt, LASfilter* const lasfilter = nullptr, AttributeAccessor * const accessor = nullptr) const;
  bool query(const Shape* const shape, std::vector<PointLAS>& addr, LASfilter* const lasfilter = nullptr, AttributeAccessor* const accessor = nullptr) const;
  bool query(const std::vector<Interval>& intervals, std::vector<PointLAS>& addr, LASfilter* const lasfilter = nullptr, AttributeAccessor* const accessor = nullptr) const;
  bool knn(const double* x, int k, double radius_max, std::vector<PointLAS>& res, LASfilter* const lasfilter = nullptr, AttributeAccessor* const accessor = nullptr) const;

  // Spatial queries
  void set_inside(Shape* shape);

  // Non spatial queries
  void set_intervals_to_read(const std::vector<Interval>& intervals);

  // Tools
  static int get_point_data_record_length(int point_data_format, int num_extrabytes = 0);
  static int get_header_size(int minor_version);
  static int guess_point_data_format(bool has_gps, bool has_rgb, bool has_nir);

private:
  void clean_index();
  void reindex();
  void clean_query();
  bool alloc_buffer();
  bool realloc_buffer();
  U64 get_true_number_of_points() const;

  // Access to point attributes without copying the whole point
  inline double get_x(const unsigned char* buf) const;
  inline double get_y(const unsigned char* buf) const;
  inline double get_z(const unsigned char* buf) const;
  inline double get_gpstime(const unsigned char* buf) const;
  inline unsigned char get_scanner_channel(const unsigned char* buf) const;
  inline unsigned char get_return_number(const unsigned char* buf) const;

public:
  LASpoint point;
  size_t npoints;
  size_t current_point;
  LASheader* header;

private:
  unsigned char* buffer;
  size_t capacity; // capacity of the buffer in bytes
  int next_point;

  bool own_header; // The pointer to the LASheader is owned by LASlib (LASreader) but could, in some cases, be owned by the class

  // For spatial indexed search
  GridPartition* index;
  int current_interval;
  std::vector<Interval> intervals_to_read;
  bool read_started;
  bool inside;
  Shape* shape;
  std::string file;

public:
  enum attribute_type { UNDOC = 0, UCHAR = 1, CHAR = 2, USHORT = 3, SHORT = 4, ULONG = 5, LONG = 6, ULONGLONG = 7, LONGLONG = 8, FLOAT = 9, DOUBLE = 10};
  enum attributes{X, Y, Z, I, T, RN, NOR, SDF, EoF, CLASS, SYNT, KEYP, WITH, OVER, UD, SA, PSID, R, G, B, NIR, CHAN, BUFF};
};

#endif
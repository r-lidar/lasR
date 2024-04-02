#ifndef LASF_H
#define LASF_H

#include "laszip.hpp"
#include "laspoint.hpp"
#include "lasdefinitions.hpp"

#include "Interval.h"
#include "Shape.h"

#include <vector>
#include <string>

class GridPartition;
class Raster;
class LASfilter;
class LAStransform;

class LAS
{
public:
  LAS(LASheader* header);
  LAS(const Raster& raster);
  ~LAS();
  bool add_attribute( int data_type, const std::string& name, const std::string& description, double scale, double offset);
  bool add_point(const LASpoint& p);
  bool add_rgb();
  bool seek(int pos);
  bool read_point(bool include_withhelded = false);
  void set_file(const std::string& file) { this->file = file; };
  void update_point();
  void remove_point();
  bool is_indexed() { return index != 0; };
  bool is_attribute_loadable(int index);

  // Thread safe
  void get_xyz(int pos, double* xyz) const;
  bool get_point(int pos, PointLAS& pt, LASfilter* const lasfilter = nullptr, LAStransform* const lastransform = nullptr) const;
  bool query(const Shape* const shape, std::vector<PointLAS>& addr, LASfilter* const lasfilter = nullptr, LAStransform* const lastransform = nullptr) const;
  bool query(const std::vector<Interval>& intervals, std::vector<PointLAS>& addr, LASfilter* const lasfilter = nullptr, LAStransform* const lastransform = nullptr) const;

  // Spatial queries
  void set_inside(Shape* shape);

  // Non spatial queries
  void set_intervals_to_read(const std::vector<Interval>& intervals);

  // Tools
  LAStransform* make_z_transformer(const std::string& use_attribute);

private:
  void clean_index();
  void clean_query();
  bool update_point_and_buffer();

public:
  LASpoint point;
  int npoints;
  int current_point;
  LASheader* header;

private:
  unsigned char* buffer;
  int capacity; // capacity of the buffer in bytes
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
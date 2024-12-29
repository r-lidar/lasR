#ifndef HEADER_H
#define HEADER_H

#include "PointSchema.h"
#include <cinttypes>


class Header
{
public:
  // General purpose
  std::string signature;

  double max_x, min_x, max_y, min_y, max_z, min_z;
  bool spatial_index;
  uint64_t number_of_point_records;
  AttributeSchema schema;
  CRS crs;

  // LAS specification
  double x_scale_factor, y_scale_factor, z_scale_factor;
  double x_offset, y_offset, z_offset;
  double gpstime; // first point only
  unsigned short file_creation_year, file_creation_day;
  bool adjusted_standard_gps_time;


  Header();
  void dump() const;
  inline double area() const { return (max_x-min_y)*(max_y-min_y); }
  inline double density() const { return (double)number_of_point_records/area(); }
  void add_attribute(const Attribute& attr);
  std::pair<unsigned short, unsigned short> gpstime_date() const;
};

#endif
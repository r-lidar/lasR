#ifndef HEADER_H
#define HEADER_H

#include "PointSchema.h"


class Header
{
public:
  double max_x, min_x, max_y, min_y, max_z, min_z;
  double x_scale_factor, y_scale_factor, z_scale_factor;
  double x_offset, y_offset, z_offset;
  double gpstime; // first point
  unsigned short file_creation_year, file_creation_day;
  bool adjusted_standard_gps_time;
  bool spatial_index;
  uint64_t number_of_point_records;
  AttributeSchema schema;
  CRS crs;

  Header();
  void dump() const;
  void add_attribute(const Attribute& attr);
};

#endif
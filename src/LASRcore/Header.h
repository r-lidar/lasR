#ifndef HEADER_H
#define HEADER_H

// If NOGDALCRS is defined it allows to compile a subset of the code without linking to GDAL
// for example to use lasr in third party software. In this case stages with GDAL wont be usable but
// PointCloud class will be

#include "PointSchema.h"

#ifndef NOGDALCRS
#include "CRS.h"
#endif

class Header
{
public:
  // General purpose
  std::string signature;
  unsigned char version_major;
  unsigned char version_minor;

  // General purpose
  double max_x, min_x, max_y, min_y, max_z, min_z;
  bool spatial_index;
  uint64_t number_of_point_records;
  AttributeSchema schema;

  void set_crs(int);
  void set_crs(const std::string&);
  std::string get_wkt() const;

#ifndef NOGDALCRS
  CRS crs;
#else
  int epsg;
  std::string wkt;
#endif

  // LAS specifications only
  double x_scale_factor, y_scale_factor, z_scale_factor;
  double x_offset, y_offset, z_offset;
  double gpstime; // first point only
  unsigned char point_data_format;
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
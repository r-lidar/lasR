#include "Header.h"
#include "print.h"

Header::Header()
{
  max_x = 0;
  min_x = 0;
  max_y = 0;
  min_y = 0;
  max_z = 0;
  min_z = 0;
  x_scale_factor = 1;
  y_scale_factor = 1;
  z_scale_factor = 1;
  x_offset = 0;
  y_offset = 0;
  z_offset = 0;
  gpstime = 0;
  file_creation_year = 0;
  file_creation_day = 0;
  adjusted_standard_gps_time = false;
  spatial_index = false;
  number_of_point_records = 0;
  schema = AttributeSchema();
  crs = CRS();
}

// # nocov start
void Header::dump() const
{
  printf("Max Coordinates: (%.2f, %.2f, %.2f)\n", max_x, max_y, max_z);
  printf("Min Coordinates: (%.2f, %.2f, %.2f)\n", min_x, min_y, min_z);
  printf("Scale Factors: (%.2f, %.2f, %.2f)\n", x_scale_factor, y_scale_factor, z_scale_factor);
  printf("Offsets: (%.2f, %.2f, %.2f)\n", x_offset, y_offset, z_offset);
  printf("GPS Time: %.2f\n", gpstime);
  printf("File Creation Year: %u\n", file_creation_year);
  printf("File Creation Day: %u\n", file_creation_day);
  printf("Adjusted Standard GPS Time: %s\n", adjusted_standard_gps_time ? "true" : "false");
  printf("Spatial Index: %s\n", spatial_index ? "true" : "false");
  printf("Number of Point Records: %lu\n", number_of_point_records);
  schema.dump();
  crs.dump();
}
// # nocov end

void Header::add_attribute(const Attribute& attr)
{
  schema.add_attribute(attr);
}
#include "Header.h"
#include "print.h"

Header::Header()
{
  // General purpose
  version_major = 0;
  version_minor = 0;

  // General purpose
  max_x = 0;
  min_x = 0;
  max_y = 0;
  min_y = 0;
  max_z = 0;
  min_z = 0;

  // LAS specifications only
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
  point_data_format = 0;

  // General purpose
  spatial_index = false;
  number_of_point_records = 0;
  schema = AttributeSchema();
  crs = CRS();
}

// # nocov start
void Header::dump() const
{
  print("%s: %u.%u\n", signature.c_str(), version_major, version_minor);
  print("Max Coordinates: (%.2f, %.2f, %.2f)\n", max_x, max_y, max_z);
  print("Min Coordinates: (%.2f, %.2f, %.2f)\n", min_x, min_y, min_z);
  print("Scale Factors: (%.2f, %.2f, %.2f)\n", x_scale_factor, y_scale_factor, z_scale_factor);
  print("Offsets: (%.2f, %.2f, %.2f)\n", x_offset, y_offset, z_offset);
  print("GPS Time: %.2f\n", gpstime);
  print("File Creation Year: %u\n", file_creation_year);
  print("File Creation Day: %u\n", file_creation_day);
  print("Adjusted Standard GPS Time: %s\n", adjusted_standard_gps_time ? "true" : "false");
  print("Spatial Index: %s\n", spatial_index ? "true" : "false");
  print("Number of Point Records: %" PRIu64 "\n", number_of_point_records);
  schema.dump(true);
  crs.dump();
}
// # nocov end

void Header::add_attribute(const Attribute& attr)
{
  schema.add_attribute(attr);
}

#include <ctime>

// Define macros for cross-platform compatibility
#ifdef _WIN32
#define timegm _mkgmtime
inline void gmtime_r(const time_t* timep, std::tm* result)
{
  gmtime_s(result, timep);
}
#endif

std::pair<unsigned short, unsigned short> Header::gpstime_date() const
{
  if (gpstime != 0 && adjusted_standard_gps_time)
  {
    uint64_t ns = ((uint64_t)gpstime+1000000000ULL)*1000000000ULL + 315964800000000000ULL; // offset between gps epoch and unix epoch is 315964800 seconds

    struct timespec ts;
    ts.tv_sec = ns / 1000000000ULL;
    ts.tv_nsec = ns % 1000000000ULL;

    struct tm stm;
    gmtime_r(&ts.tv_sec, &stm);

    return {stm.tm_year + 1900, stm.tm_yday};

    //std::cout << stm.tm_year + 1900 << "-" << stm.tm_mon + 1 << "-" << stm.tm_mday << " " << stm.tm_hour << ":" << stm.tm_min << ":" << stm.tm_sec << std::endl;
  }
  else
  {
    return {0, 0};
  }
}
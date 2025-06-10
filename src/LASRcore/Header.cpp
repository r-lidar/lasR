#include "Header.h"
#include "print.h"

#include <cinttypes> // PRIu64

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

#ifndef NOGDAL
  crs = CRS();
#endif
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

#ifndef NOGDAL
  crs.dump();
#endif
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

void Header::set_crs(int epsg)
{
#ifndef NOGDAL
  crs = CRS(epsg);
#else
  this->epsg = epsg;
#endif
}

void Header::set_crs(const std::string& wkt)
{
#ifndef NOGDAL
  crs = CRS(wkt);
#else
  this->wkt = wkt;
#endif
}

std::string Header::get_wkt() const
{
#ifndef NOGDAL
  return(crs.get_wkt());
#else
  return(wkt);
#endif
}

// Default constructor
VLR::VLR()
{
  reserved = 0;
  record_id = 0;
  record_length_after_header = 0;
  std::memset(user_id, 0, sizeof(user_id));
  std::memset(description, 0, sizeof(description));
  data = nullptr;
}

// Copy constructor
VLR::VLR(const VLR& other)
{
  reserved = other.reserved;
  record_id = other.record_id;
  record_length_after_header = other.record_length_after_header;
  std::memcpy(user_id, other.user_id, sizeof(user_id));
  std::memcpy(description, other.description, sizeof(description));
  data = nullptr;

  if (other.record_length_after_header > 0 && other.data)
  {
    data = new unsigned char[other.record_length_after_header];
    std::memcpy(data, other.data, other.record_length_after_header);
  }
}

// Copy assignment operator
VLR& VLR::operator=(const VLR& other)
{
  if (this == &other) return *this;  // Self-assignment check

  // Clean up old data
  delete[] data;

  reserved = other.reserved;
  record_id = other.record_id;
  record_length_after_header = other.record_length_after_header;
  std::memcpy(user_id, other.user_id, sizeof(user_id));
  std::memcpy(description, other.description, sizeof(description));

  // Allocate new data
  data = nullptr;
  if (other.record_length_after_header > 0 && other.data)
  {
    data = new unsigned char[other.record_length_after_header];
    std::memcpy(data, other.data, other.record_length_after_header);
  }

  return *this;
}

// Move constructor
VLR::VLR(VLR&& other) noexcept
{
  reserved = other.reserved;
  record_id = other.record_id;
  record_length_after_header = other.record_length_after_header;
  std::memcpy(user_id, other.user_id, sizeof(user_id));
  std::memcpy(description, other.description, sizeof(description));
  data = other.data;

  // Null out the source object
  other.data = nullptr;
  other.record_length_after_header = 0;
}

// Move assignment operator
VLR& VLR::operator=(VLR&& other) noexcept
{
  if (this == &other) return *this;  // Self-assignment check

  // Clean up old data
  delete[] data;

  reserved = other.reserved;
  record_id = other.record_id;
  record_length_after_header = other.record_length_after_header;
  std::memcpy(user_id, other.user_id, sizeof(user_id));
  std::memcpy(description, other.description, sizeof(description));

  // Steal data from the source object
  data = other.data;
  other.data = nullptr;
  other.record_length_after_header = 0;

  return *this;
}

// Destructor
VLR::~VLR()
{
  delete[] data;  // Free allocated memory
}


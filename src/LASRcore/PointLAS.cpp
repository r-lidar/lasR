#include "laszip.hpp"
#include "laspoint.hpp"

#include "PointLAS.h"
#include "NA.h"

#include <limits>
#include <cstring>

PointLAS::PointLAS()
{
  memset((void*)this, 0, sizeof(PointLAS));
}

PointLAS::PointLAS(const LASpoint* const p)
{
  extrabytes = nullptr;
  copy(p);
}

// Copy constructor
PointLAS::PointLAS(const PointLAS& other) : PointXYZ(other)
{
  std::memcpy((void*)this, (void*)&other, sizeof(PointLAS));
  if (other.extrabytes) extrabytes = new std::unordered_map<std::string, double>(*other.extrabytes);
}

// Move constructor
PointLAS::PointLAS(PointLAS&& other) noexcept
{
  std::memcpy((void*)this, (void*)&other, sizeof(PointLAS));
  other.extrabytes = nullptr;
}

// Copy assignment operator
PointLAS& PointLAS::operator=(const PointLAS& other)
{
  if (this == &other) return *this;

  delete extrabytes;
  std::memcpy((void*)this, (void*)&other, sizeof(PointLAS));
  if (other.extrabytes) extrabytes = new std::unordered_map<std::string, double>(*other.extrabytes);

  return *this;
}

// Move assignment operator
PointLAS& PointLAS::operator=(PointLAS&& other) noexcept
{
  if (this == &other)  return *this;

  delete extrabytes;
  std::memcpy((void*)this, (void*)&other, sizeof(PointLAS));
  extrabytes = other.extrabytes;
  other.extrabytes = nullptr;

  return *this;
}

PointLAS::~PointLAS()
{
  delete extrabytes;
  extrabytes = nullptr;
}

void PointLAS::copy(const LASpoint* const p)
{
  FID = 0;
  x = p->get_x();
  y = p->get_y();
  z = p->get_z();
  intensity = p->get_intensity();
  return_number = p->get_return_number_uni();
  number_of_returns = p->get_number_of_returns_uni();
  scan_direction_flag = p->get_scan_direction_flag();
  edge_of_flight_line = p->get_edge_of_flight_line();
  classification = p->get_classification(); // extended classification for point formats 6+
  synthetic_flag = p->get_synthetic_flag();
  keypoint_flag = p->get_keypoint_flag();
  withheld_flag = p->get_withheld_flag();
  overlap_flag =  p->get_extended_overlap_flag();
  scan_angle = p->get_scan_angle(); // degrees, scan angle rank for point formats < 6
  user_data = p->get_user_data();
  point_source_ID = p->get_point_source_ID();
  gps_time = p->get_gps_time();
  R = p->get_R();
  G = p->get_G();
  B = p->get_B();
  NIR = p->get_NIR();

  for (int i = 0 ; i < p->attributer->number_attributes ; i++)
  {
    std::string name = p->get_attribute_name(i);
    double value = p->get_attribute_as_float(i);
    if (p->attributer->attributes[i].has_no_data())
    {
      if (value == *(double*)(p->attributer->attributes[i].no_data))
      {
        value = std::numeric_limits<double>::quiet_NaN();
      }
    }

    if (extrabytes == nullptr)
      extrabytes = new std::unordered_map<std::string, double>();

    (*extrabytes)[name] = value;
  }
}

double PointLAS::get_extrabyte(const std::string& name) const
{
  double val = std::numeric_limits<double>::quiet_NaN();
  if (extrabytes)
  {
    auto it = extrabytes->find(name);
    if (it != extrabytes->end()) val = it->second;
  }
  return val;
}
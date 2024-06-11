#include "laszip.hpp"
#include "laspoint.hpp"

#include "PointLAS.h"
#include "NA.h"

#include <limits>

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
  FID = other.FID;
  intensity = other.intensity;
  return_number = other.return_number;
  number_of_returns = other.number_of_returns;
  scan_direction_flag = other.scan_direction_flag;
  edge_of_flight_line = other.edge_of_flight_line;
  classification = other.classification;
  synthetic_flag = other.synthetic_flag;
  keypoint_flag = other.keypoint_flag;
  withheld_flag = other.withheld_flag;
  overlap_flag = other.overlap_flag;
  scan_angle = other.scan_angle;
  user_data = other.user_data;
  point_source_ID = other.point_source_ID;
  gps_time = other.gps_time;
  R = other.R;
  G = other.G;
  B = other.B;
  NIR = other.NIR;
  extrabytes = other.extrabytes ? new std::unordered_map<std::string, double>(*other.extrabytes) : nullptr;
}

// Move constructor
PointLAS::PointLAS(PointLAS&& other) noexcept : PointXYZ(std::move(other))
{
  FID = other.FID;
  intensity = other.intensity;
  return_number = other.return_number;
  number_of_returns = other.number_of_returns;
  scan_direction_flag = other.scan_direction_flag;
  edge_of_flight_line = other.edge_of_flight_line;
  classification = other.classification;
  synthetic_flag = other.synthetic_flag;
  keypoint_flag = other.keypoint_flag;
  withheld_flag = other.withheld_flag;
  overlap_flag = other.overlap_flag;
  scan_angle = other.scan_angle;
  user_data = other.user_data;
  point_source_ID = other.point_source_ID;
  gps_time = other.gps_time;
  R = other.R;
  G = other.G;
  B = other.B;
  NIR = other.NIR;
  extrabytes = other.extrabytes;
  other.extrabytes = nullptr;
}

// Copy assignment operator
PointLAS& PointLAS::operator=(const PointLAS& other)
{
  if (this == &other)
  {
    return *this;
  }

  PointXYZ::operator=(other);

  FID = other.FID;
  intensity = other.intensity;
  return_number = other.return_number;
  number_of_returns = other.number_of_returns;
  scan_direction_flag = other.scan_direction_flag;
  edge_of_flight_line = other.edge_of_flight_line;
  classification = other.classification;
  synthetic_flag = other.synthetic_flag;
  keypoint_flag = other.keypoint_flag;
  withheld_flag = other.withheld_flag;
  overlap_flag = other.overlap_flag;
  scan_angle = other.scan_angle;
  user_data = other.user_data;
  point_source_ID = other.point_source_ID;
  gps_time = other.gps_time;
  R = other.R;
  G = other.G;
  B = other.B;
  NIR = other.NIR;

  delete extrabytes;
  extrabytes = other.extrabytes ? new std::unordered_map<std::string, double>(*other.extrabytes) : nullptr;

  return *this;
}

// Move assignment operator
PointLAS& PointLAS::operator=(PointLAS&& other) noexcept
{
  if (this == &other)
  {
    return *this;
  }

  PointXYZ::operator=(std::move(other));

  FID = other.FID;
  intensity = other.intensity;
  return_number = other.return_number;
  number_of_returns = other.number_of_returns;
  scan_direction_flag = other.scan_direction_flag;
  edge_of_flight_line = other.edge_of_flight_line;
  classification = other.classification;
  synthetic_flag = other.synthetic_flag;
  keypoint_flag = other.keypoint_flag;
  withheld_flag = other.withheld_flag;
  overlap_flag = other.overlap_flag;
  scan_angle = other.scan_angle;
  user_data = other.user_data;
  point_source_ID = other.point_source_ID;
  gps_time = other.gps_time;
  R = other.R;
  G = other.G;
  B = other.B;
  NIR = other.NIR;

  delete extrabytes;
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
  return_number = (p->is_extended_point_type()) ? p->get_extended_return_number() : p->get_return_number();
  number_of_returns = (p->is_extended_point_type()) ? p->get_extended_number_of_returns() : p->get_number_of_returns();
  scan_direction_flag = p->get_scan_direction_flag();
  edge_of_flight_line = p->get_edge_of_flight_line();
  classification = (p->is_extended_point_type()) ? p->get_extended_classification() : p->get_classification();
  synthetic_flag = p->get_synthetic_flag();
  keypoint_flag = p->get_keypoint_flag();
  withheld_flag = p->get_withheld_flag();
  overlap_flag =  p->get_extended_overlap_flag();
  scan_angle = (p->is_extended_point_type()) ? p->get_scan_angle() : p->get_scan_angle_rank();
  user_data = p->get_user_data();
  point_source_ID = p->get_point_source_ID();
  gps_time = p->get_gps_time();
  R = p->get_R();
  G = p->get_R();
  B = p->get_R();
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
    i++;
  }
}

double PointLAS::get_extrabyte(const std::string& name) const
{
  int val = NA_F32_RASTER;
  if (extrabytes)
  {
    auto it = extrabytes->find(name);
    if (it != extrabytes->end()) val = it->second;
  }
  return val;
}
#ifndef POINTLAS_H
#define POINTLAS_H

#include "Shape.h"

#include <string>

class LASpoint;

struct PointLAS : public PointXYZ
{
  PointLAS();
  PointLAS(const LASpoint* const p);

  PointLAS(const PointLAS& other); // Copy constructor
  PointLAS(PointLAS&& other) noexcept; // Move constructor

  PointLAS& operator=(const PointLAS& other); // Copy assignment operator
  PointLAS& operator=(PointLAS&& other) noexcept; // Move assignment operator

  ~PointLAS();
  double get_extrabyte(const std::string& name) const;

  void copy(const LASpoint* const p);
  unsigned int FID;
  unsigned short intensity;
  unsigned char return_number;
  unsigned char number_of_returns;
  bool scan_direction_flag;
  bool edge_of_flight_line;
  unsigned char classification;
  bool synthetic_flag;
  bool keypoint_flag;
  bool withheld_flag;
  bool overlap_flag;
  float scan_angle;
  char user_data;
  unsigned short point_source_ID;
  double gps_time;
  unsigned short R;
  unsigned short G;
  unsigned short B;
  unsigned short NIR;
  std::unordered_map<std::string, double>* extrabytes;
};

#endif
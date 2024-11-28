#ifndef LASACCESSOR_H
#define LASACCESSOR_H

#include <functional>
#include <string>

#include "laszip.hpp"
#include "laspoint.hpp"

class LASaccessor
{
public:
  LASaccessor() { accessor = nullptr; attribute_index = -1; }
  LASaccessor(std::string s) : LASaccessor()
  {
    attribute_name = s;

    if      (attribute_name == "X")                accessor = [](const LASpoint* point) { return static_cast<double>(point->get_x()); };
    else if (attribute_name == "Y")                accessor = [](const LASpoint* point) { return static_cast<double>(point->get_y()); };
    else if (attribute_name == "Z")                accessor = [](const LASpoint* point) { return static_cast<double>(point->get_z()); };
    else if (attribute_name == "Intensity")        accessor = [](const LASpoint* point) { return static_cast<double>(point->get_intensity()); };
    else if (attribute_name == "ReturnNumber")     accessor = [](const LASpoint* point) { return static_cast<double>(point->get_return_number()); };
    else if (attribute_name == "NumberOfReturns")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_number_of_returns()); };
    else if (attribute_name == "Classification")   accessor = [](const LASpoint* point) { return static_cast<double>(point->get_classification()); };
    else if (attribute_name == "PointSourceID")    accessor = [](const LASpoint* point) { return static_cast<double>(point->get_point_source_ID()); };
    else if (attribute_name == "gpstime")          accessor = [](const LASpoint* point) { return static_cast<double>(point->get_gps_time()); };
    else if (attribute_name == "ScanAngle")        accessor = [](const LASpoint* point) { return static_cast<double>(point->get_scan_angle()); };
    else if (attribute_name == "UserData")         accessor = [](const LASpoint* point) { return static_cast<double>(point->get_user_data()); };
    else if (attribute_name == "R")                accessor = [](const LASpoint* point) { return static_cast<double>(point->get_R()); };
    else if (attribute_name == "G")                accessor = [](const LASpoint* point) { return static_cast<double>(point->get_G()); };
    else if (attribute_name == "B")                accessor = [](const LASpoint* point) { return static_cast<double>(point->get_B()); };
    else if (attribute_name == "NIR")              accessor = [](const LASpoint* point) { return static_cast<double>(point->get_NIR()); };
    else
    {
      accessor = [this](const LASpoint* point)
      {
        if (this->attribute_index < 0)
        {
          if (this->attribute_index == -2)
            return 0.0;

          this->attribute_index = point->attributer->get_attribute_index(this->attribute_name.c_str());

          if (this->attribute_index == -1)
          {
            this->attribute_index = -2;
            warning("No attribute '%s' for this point cloud. Returning 0 for this attribute may have unexpected side effect\n", this->attribute_name.c_str());
            return 0.0;
          }
        }

        return static_cast<double>(point->get_attribute_as_float(this->attribute_index));
      };
    }
  };

  double operator()(LASpoint* point)
  {
    return accessor(point);
  }

protected:
  std::function<double(const LASpoint*)> accessor;
  std::string attribute_name;
  int attribute_index;
};

#endif
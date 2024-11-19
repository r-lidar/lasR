#ifndef ATTRIBUTEACCESSORS_H
#define ATTRIBUTEACCESSORS_H

#include <functional>
#include <string>

#include "laspoint.hpp"

#include "AttributeMapping.h"

class AttributeAccessor
{
public:
  AttributeAccessor() { accessor = nullptr; attribute_index = -1; }
  AttributeAccessor(std::string s) : AttributeAccessor()
  {
    this->attribute_name = map_attribute(s);

    if      (attribute_name == "x")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_x()); };
    else if (attribute_name == "y")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_y()); };
    else if (attribute_name == "z")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_z()); };
    else if (attribute_name == "i")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_intensity()); };
    else if (attribute_name == "r")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_return_number()); };
    else if (attribute_name == "n")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_number_of_returns()); };
    else if (attribute_name == "c")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_classification()); };
    else if (attribute_name == "p")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_point_source_ID()); };
    else if (attribute_name == "t")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_gps_time()); };
    else if (attribute_name == "a")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_scan_angle()); };
    else if (attribute_name == "u")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_user_data()); };
    else if (attribute_name == "R")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_R()); };
    else if (attribute_name == "G")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_G()); };
    else if (attribute_name == "B")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_B()); };
    else if (attribute_name == "NIR")  accessor = [](const LASpoint* point) { return static_cast<double>(point->get_NIR()); };
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
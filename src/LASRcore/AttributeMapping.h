#ifndef ATTRIBUTEMAPPING_H
#define ATTRIBUTEMAPPING_H

#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>

static const std::unordered_map<std::string, std::vector<std::string>> attribute_map = {
  {"z", {"Z", "z"}},
  {"i", {"Intensity", "intensity", "i"}},
  {"r", {"return", "Return", "ReturnNumber", "return_number", "r"}},
  {"n", {"NumberOfReturn", "NumberReturn", "numberofreturn", "n"}},
  {"c", {"Classification", "classification", "class", "c"}},
  {"t", {"gpstime", "gps_time", "GPStime", "t", "time", "gps"}},
  //s
  //k
  //w
  {"u", {"UserData", "userdata", "user_data", "ud", "u"}},
  {"p", {"PointSourceID", "point_source", "point_source_id", "pointsourceid", "psid", "p"}},
  //e
  //d
  {"a", {"angle", "Angle", "ScanAngle", "ScanAngleRank", "scan_angle", "a"}},
  {"R", {"R", "Red", "red"}},
  {"G", {"G", "Green", "green"}},
  {"B", {"B", "Blue", "blue"}},
  {"N", {"N", "NIR", "nir"}},
};

static std::string map_attribute(const std::string& attribute)
{
  for (const auto& [standard_name, aliases] : attribute_map)
  {
    if (std::find(aliases.begin(), aliases.end(), attribute) != aliases.end())
    {
      return standard_name;
    }
  }

  return attribute;
}

#endif
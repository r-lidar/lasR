#ifndef FILTERPARSER_H
#define FILTERPARSER_H

#include <string>
#include <unordered_map>
#include <vector>
#include <regex>
#include <stdexcept>
#include <algorithm>

class FilterParser
{
private:
  const std::unordered_map<std::string, std::vector<std::string>> attributeMapping = {
    {"z", {"Z", "z", "height"}},
    {"intensity", {"Intensity", "intensity", "i"}},
    {"class", {"Classification", "classification", "class"}},
    {"userdata", {"UserData", "userdata", "user_data", "ud"}},
    {"psid", {"PointSourceID", "point_source", "point_source_id", "pointsourceid", "psid"}},
    {"gpstime", {"gpstime", "gps_time", "GPStime", "t", "time", "gps"}},
    {"angle", {"angle", "Angle", "ScanAngle", "ScanAngleRank", "scan_angle", "a"}},
    {"return", {"return", "Return", "ReturnNumber", "return_number", "r"}}
  };

  const std::vector<std::pair<std::string, std::regex>> patterns = {
    {"above", std::regex(R"(^([A-Za-z_]+)\s*>\s*([0-9.]+)$)")},
    {"lower", std::regex(R"(^([A-Za-z_]+)\s*<\s*([0-9.]+)$)")},
    {"aboveeq", std::regex(R"(^([A-Za-z_]+)\s*>=\s*([0-9.]+)$)")},
    {"lowereq", std::regex(R"(^([A-Za-z_]+)\s*<=\s*([0-9.]+)$)")},
    {"equal", std::regex(R"(^([A-Za-z_]+)\s*==\s*([0-9.]+)$)")},
    {"different", std::regex(R"(^([A-Za-z_]+)\s*!=\s*([0-9.]+)$)")},
    {"in", std::regex(R"(^([A-Za-z_]+)\s*%in%\s*([0-9.]+(?:\s+[0-9.]+)*)$)")},
    {"out", std::regex(R"(^([A-Za-z_]+)\s*%out%\s*([0-9.]+(?:\s+[0-9.]+)*)$)")},
    {"between", std::regex(R"(^([A-Za-z_]+)\s*%between%\s*([0-9.]+)\s+([0-9.]+)$)")}
  };

  // Helper function to map an attribute to its standardized name
  std::string map_attribute(const std::string& attribute) const;

public:
  FilterParser() = default;
  ~FilterParser() = default;
  std::string parse(const std::string& condition) const;
};

#endif // FILTERPARSER_H

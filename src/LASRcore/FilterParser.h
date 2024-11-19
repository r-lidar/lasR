#ifndef FILTERPARSER_H
#define FILTERPARSER_H

#include <string>
#include <vector>
#include <regex>

class FilterParser
{
private:
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

public:
  FilterParser() = default;
  ~FilterParser() = default;
  std::string parse(const std::string& condition) const;
};

#endif // FILTERPARSER_H

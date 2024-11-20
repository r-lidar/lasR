#ifndef FILTERPARSER_H
#define FILTERPARSER_H

#include <string>
#include <vector>

class FilterParser
{
private:
  std::string trim(const std::string& str) const;
  std::vector<std::string> split(const std::string& str, char delimiter) const;

public:
  FilterParser() = default;
  ~FilterParser() = default;
  std::string parse(const std::string& condition) const;
};

#endif // FILTERPARSER_H

#ifndef POINTFILTER_HPP
#define POINTFILTER_HPP

#include "PointSchema.h"

#include <string>
#include <vector>

class Condition : public AttributeHandler
{
public:
  Condition() : AttributeHandler() {};
  Condition(const std::string& attribute_name) : AttributeHandler(attribute_name) {};
  virtual bool filter(const Point* point) = 0;
  virtual ~Condition(){};
};

class FilterParser
{
private:
  std::string trim(const std::string& str) const;
  std::vector<std::string> split(const std::string& str, char delimiter) const;

public:
  FilterParser() = default;
  ~FilterParser() = default;
  Condition* parse(const std::string& condition) const;
};

class PointFilter
{
public:
  bool filter(const Point* point);
  void add_condition(const std::string& x);
  void add_condition(Condition* condition);

  PointFilter() = default;
  ~PointFilter();

private:
  std::vector<Condition*> conditions;
};


#endif

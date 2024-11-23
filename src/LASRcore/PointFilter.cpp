#include "PointFilter.h"

#include <numeric>
#include <stdexcept>

class ConditionKeepBelow : public Condition
{
public:
  ConditionKeepBelow(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return readAccessor(point) >= threshold; }
private:
  double threshold;
};

class ConditionKeepBelowEqual : public Condition
{
public:
  ConditionKeepBelowEqual(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return readAccessor(point) > threshold; }
private:
  double threshold;
};

class ConditionKeepAbove : public Condition
{
public:
  ConditionKeepAbove(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return readAccessor(point) <= threshold; }
private:
  double threshold;
};

class ConditionKeepAboveEqual : public Condition
{
public:
  ConditionKeepAboveEqual(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return readAccessor(point) < threshold; }
private:
  double threshold;
};

class ConditionKeepBetween : public Condition
{
public:
  ConditionKeepBetween(const std::string& attribute_name, double below, double above) : Condition(attribute_name)
  {
    if (below > above)
    {
      this->below = above;
      this->above = below;
    }
    else
    {
      this->below = below;
      this->above = above;
    }
  }
  inline bool filter(const Point* point) { double v = readAccessor(point); return (v < below) || (v >= above);  }
private:
  double below;
  double above;
};

class ConditionKeepEqual : public Condition
{
public:
  ConditionKeepEqual(const std::string& attribute_name, double value) : Condition(attribute_name) { this->value = value; }
  inline bool filter(const Point* point) { return readAccessor(point) != value; }
private:
  double value;
};

class ConditionKeepDifferent : public Condition
{
public:
  ConditionKeepDifferent(const std::string& attribute_name, double value) : Condition(attribute_name) { this->value = value; }
  inline bool filter(const Point* point) { return readAccessor(point) == value; }
private:
  double value;
};

class ConditionKeepIn : public Condition
{
public:
  ConditionKeepIn(const std::string& attribute_name, double* values, int size) : Condition(attribute_name)
  {
    memcpy(this->values, values, size*sizeof(double));
    this->size = size;
  }
  inline bool filter(const Point* point)
  {
    double v = readAccessor(point);
    for (int i = 0 ; i < size ; i++)
    {
      if (values[i] == v)
        return false;
    }
    return true;
  }
private:
  double values[64];
  int size;
};

class ConditionKeepOut : public Condition
{
public:
  ConditionKeepOut(const std::string& attribute_name, double* values, int size) : Condition(attribute_name)
  {
    memcpy(this->values, values, size*sizeof(double));
    this->size = size;
  }
  inline bool filter(const Point* point)
  {
    double v = readAccessor(point);
    for (int i = 0 ; i < size ; i++)
    {
      if (values[i] == v)
        return true;
    }
    return false;
  }
private:
  double values[64];
  int size;
};

bool PointFilter::filter(const Point* point)
{
  for (const auto condition : conditions)
  {
    if (condition->filter(point))
      return TRUE; // point was filtered
  }
  return FALSE; // point survived
}

PointFilter::~PointFilter()
{
  for (auto condition : conditions) delete condition;
}

void PointFilter::add_condition(Condition* condition)
{
  if (condition != nullptr)
    conditions.push_back(condition);
}

void PointFilter::add_condition(const std::string& x)
{
  FilterParser fp;
  auto condition = fp.parse(x);
  add_condition(condition);
}

Condition* FilterParser::parse(const std::string& condition) const
{
  if (condition.empty() || condition[0] == '-')
  {
    return nullptr; // Already a flag, return as-is
  }

  static const std::vector<std::string> operators = {"%between%", "%in%", "%out%", "==", "!=", ">=", "<=", ">", "<"};

  // Find the operator in the condition
  std::string op;
  size_t opPos = std::string::npos;
  for (const auto& candidate : operators)
  {
    opPos = condition.find(candidate);
    if (opPos != std::string::npos)
    {
      op = candidate;
      break;
    }
  }

  if (op.empty())
  {
    throw std::invalid_argument("Invalid condition: no operator found");
  }

  // Extract left-hand side (LHS), operator, and right-hand side (RHS)
  std::string lhs = trim(condition.substr(0, opPos));
  lhs = map_attribute(lhs);
  std::string rhs = trim(condition.substr(opPos + op.length()));

  // Process based on the operator
  if (op == "==")
  {
    return new ConditionKeepEqual(lhs, std::stod(rhs));
  }
  else if (op == "!=")
  {
    return new ConditionKeepDifferent(lhs, std::stod(rhs));
  }
  else if (op == ">")
  {
    return new ConditionKeepAbove(lhs, std::stod(rhs));
  }
  else if (op == "<")
  {
    return new ConditionKeepBelow(lhs, std::stod(rhs));
  }
  else if (op == ">=")
  {
    return new ConditionKeepAboveEqual(lhs, std::stod(rhs));
  }
  else if (op == "<=")
  {
    return new ConditionKeepBelowEqual(lhs, std::stod(rhs));
  }
  else if (op == "%in%")
  {
    std::vector<std::string> values = split(rhs, ' ');
    std::vector<double> doubles; doubles.reserve(values.size());
    std::transform(values.begin(), values.end(), std::back_inserter(doubles), [](const std::string& val) { return std::stod(val); });
    return new ConditionKeepIn(lhs, &doubles[0], doubles.size());
  }
  else if (op == "%out%")
  {
    std::vector<std::string> values = split(rhs, ' ');
    std::vector<double> doubles; doubles.reserve(values.size());
    std::transform(values.begin(), values.end(), std::back_inserter(doubles), [](const std::string& val) { return std::stod(val); });
    return new ConditionKeepOut(lhs, &doubles[0], doubles.size());
  }
  else if (op == "%between%")
  {
    // Expect two values for "between"
    std::vector<std::string> values = split(rhs, ' ');
    if (values.size() != 2) throw std::invalid_argument("Invalid condition: %between% must have two values");
    return new ConditionKeepBetween(lhs, std::stod(values[0]), std::stod(values[1]));
  }

  throw std::invalid_argument("Invalid condition: unsupported operator");
}

std::vector<std::string> FilterParser::split(const std::string& str, char delimiter) const
{
  std::vector<std::string> tokens;
  size_t start = 0;
  size_t end = 0;

  while ((end = str.find(delimiter, start)) != std::string::npos)
  {
    tokens.push_back(trim(str.substr(start, end - start)));
    start = end + 1;
  }

  tokens.push_back(trim(str.substr(start)));

  return tokens;
}

std::string FilterParser::trim(const std::string& str) const
{
  size_t start = str.find_first_not_of(" \t");
  size_t end = str.find_last_not_of(" \t");
  return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}


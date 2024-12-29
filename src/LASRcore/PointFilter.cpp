#include "PointFilter.h"

#include <numeric>
#include <stdexcept>

class ConditionKeepBelow : public Condition
{
public:
  ConditionKeepBelow(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return read(point) >= threshold; }
private:
  double threshold;
};

class ConditionKeepBelowEqual : public Condition
{
public:
  ConditionKeepBelowEqual(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return read(point) > threshold; }
private:
  double threshold;
};

class ConditionKeepAbove : public Condition
{
public:
  ConditionKeepAbove(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return read(point) <= threshold; }
private:
  double threshold;
};

class ConditionKeepAboveEqual : public Condition
{
public:
  ConditionKeepAboveEqual(const std::string& attribute_name, double threshold) : Condition(attribute_name) { this->threshold = threshold; }
  inline bool filter(const Point* point) { return read(point) < threshold; }
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
  inline bool filter(const Point* point) { double v = read(point); return (v < below) || (v >= above);  }
private:
  double below;
  double above;
};

class ConditionKeepEqual : public Condition
{
public:
  ConditionKeepEqual(const std::string& attribute_name, double value) : Condition(attribute_name) { this->value = value; }
  inline bool filter(const Point* point) { return read(point) != value; }
private:
  double value;
};

class ConditionKeepDifferent : public Condition
{
public:
  ConditionKeepDifferent(const std::string& attribute_name, double value) : Condition(attribute_name) { this->value = value; }
  inline bool filter(const Point* point) { return read(point) == value; }
private:
  double value;
};

class ConditionKeepIn : public Condition
{
public:
  ConditionKeepIn(const std::string& attribute_name, const std::vector<double>& values) : Condition(attribute_name)
  {
    this->values = values;
  }
  inline bool filter(const Point* point)
  {
    double v = read(point);
    for (const auto val : values)
    {
      if (val == v)
        return false;
    }
    return true;
  }
private:
  std::vector<double> values;
};

class ConditionKeepOut : public Condition
{
public:
  ConditionKeepOut(const std::string& attribute_name, const std::vector<double>& values) : Condition(attribute_name)
  {
    this->values = values;
  }
  inline bool filter(const Point* point)
  {
    double v = read(point);
    for (const auto val : values)
    {
      if (val == v)
        return true;
    }
    return false;
  }
private:
  std::vector<double> values;
};

class ConditionKeepInside : public Condition
{
public:
  ConditionKeepInside(double xmin, double ymin, double xmax, double ymax, bool circle = false) : Condition(""), xmin(xmin), ymin(ymin), xmax(xmax), ymax(ymax), circle(circle) { }
  inline bool filter(const Point* point)
  {
    double x = point->get_x();
    if (x < xmin || x > xmax) return true;
    double y = point->get_y();
    if (y < ymin || y > ymax) return true;

    if (circle)
    {
      double r2 = (xmax-xmin)/2;
      double center_x = (xmax+xmin)/2;
      double center_y = (ymax+ymin)/2;
      double dx = center_x - x;
      double dy = center_y - y;
      return ((dx*dx+dy*dy) > r2*r2);
    }

    return false;
  }
private:
  double xmin, ymin, xmax, ymax;
  bool circle;
};

class ConditionKeepRandomFraction : public Condition
{
public:
  ConditionKeepRandomFraction(double probability) : Condition(""), probability(probability) {}

  inline bool filter(const Point* point)
  {
    double random_number = static_cast<double>(std::rand()) / RAND_MAX;
    return random_number > probability;
  }

private:
  double probability;
};

bool PointFilter::filter(const Point* point) {
  for (const auto condition : conditions) {
    if (condition->filter(point))
      return true; // point was filtered
  }
  return false; // point survived
}

void PointFilter::reset() {
  for (const auto condition : conditions)
    condition->reset();
}

PointFilter::~PointFilter() {
  for (auto condition : conditions) delete condition;
}

void PointFilter::add_condition(Condition* condition)
{
  if (condition == nullptr) return;
  conditions.push_back(condition);
}

void PointFilter::add_condition(const std::string& x)
{
  FilterParser fp;
  Condition* cond = fp.parse(x);
  add_condition(cond);
}

void PointFilter::add_clip(double xmin, double ymin, double xmax, double ymax, bool circle)
{
  add_condition(new ConditionKeepInside(xmin, ymin, xmax, ymax, circle));
}

Condition* FilterParser::parse(const std::string& condition) const
{
  if (condition.empty())
    return nullptr;

  if (condition[0] == '-')
    return nullptr;

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
    return new ConditionKeepIn(lhs, doubles);
  }
  else if (op == "%out%")
  {
    std::vector<std::string> values = split(rhs, ' ');
    std::vector<double> doubles; doubles.reserve(values.size());
    std::transform(values.begin(), values.end(), std::back_inserter(doubles), [](const std::string& val) { return std::stod(val); });
    return new ConditionKeepOut(lhs, doubles);
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


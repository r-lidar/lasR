#include "Metrics.h"
#include "error.h"
#include "NA.h"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <limits>
#include <stdexcept>

bool MetricManager::parse(const std::vector<std::string>& names, bool support_streamable)
{
  if (names.size() == 0) return true;

  // Check if we have only streamable MetricManager
  if (support_streamable)
  {
    streamable = true;
    for (const auto& name : names)
    {
      if (name != "max" && name != "min" && name != "count" && name != "z_max" && name != "z_min")
      {
        streamable = false;
        break;
      }
    }
  }

  // If we have only streamable MetricManager
  if (streamable)
  {
    for (const auto& name : names)
    {
      if (name == "max" || name == "z_max")
        streaming_operators.push_back(&MetricManager::pmax);
      else if (name == "min" || name == "z_min")
        streaming_operators.push_back(&MetricManager::pmin);
      else if (name == "count")
        streaming_operators.push_back(&MetricManager::pcount);

      this->names.push_back(name);
    }

    return true;
  }

  // Regular case including non streamable MetricManager
  try
  {
    for (const auto& name : names)
    {
      regular_operators.push_back(parse(name));
      this->names.push_back(name);
    }
  }
  catch(std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  return true;
}

MetricCalculator MetricManager::parse(const std::string& name)
{
  // name is in the format attribute_functionXX where attribute is an attribute of the points
  // function is a function to apply and XX an optional parameter. We first parse the string

  float param = 0;
  std::string metric;
  std::string attribute;

  std::string::size_type underscore_pos = name.find('_');
  if (underscore_pos == std::string::npos)
  {
    attribute = "z";
    metric = name;
  }
  else
  {
    attribute = name.substr(0, underscore_pos);
    metric = name.substr(underscore_pos + 1);
  }

  if (metric[0] == 'p')
  {
    std::string probs = metric.substr(1);
    param = string_to_float(probs);

    if (std::isnan(param)) throw std::invalid_argument("Invalid parameter in: " + name);
    if (param < 0 || param > 100)  throw std::invalid_argument("Percentile out of range (0-100)");

    metric = metric[0];
  }
  else if (metric.substr(0,5) == "above")
  {
    std::string h = metric.substr(5);
    param = string_to_float(h);

    if (std::isnan(param)) throw std::invalid_argument("Invalid parameter in: " + name);

    metric = metric.substr(0,5);
  }

  // The string is parsed. We can instantiate the accessors

  auto it1 = metric_functions.find(metric);
  if (it1 == metric_functions.end()) throw std::invalid_argument("Invalid metric name: " + metric);

  PointAttributeAccessor attribute_accessor;
  auto it2 = attribute_functions.find(attribute);
  if (it2 != attribute_functions.end())
  {
    attribute_accessor = it2->second;
  }
  else
  {
    // We can't find the attribute in the list of existing attributes. This might be an extrabyte (or a typo)
    // we assume an extrabyte for now
    printf("WARNING extrabyte for %s\n", attribute.c_str());
    attribute_accessor = [attribute](const PointLAS& p) { return p.get_extrabyte(attribute); };
  }

  return MetricCalculator(it1->second, attribute_accessor, param);
}

// streamable MetricManager
float MetricManager::pmax  (float x, float y) const { if (x == NA_F32_RASTER) return y; return (x > y) ? x : y; }
float MetricManager::pmin  (float x, float y) const { if (x == NA_F32_RASTER) return y; return (x < y) ? x : y; }
float MetricManager::pcount(float x, float y) const { if (x == NA_F32_RASTER) return 1; return x+1; }

// batch MetricManager

float MetricManager::min(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  double min = std::numeric_limits<double>::max();
  for (const auto& point : points)
  {
    double val = accessor(point);
    if (min > val) min = val;
  }
  return min;
}

float MetricManager::max(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  double max = std::numeric_limits<double>::lowest();
  for (const auto& point : points)
  {
    double val = accessor(point);
    if (max < val) max = val;
  }
  return max;
}

float MetricManager::mean(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  double sum = 0.0;
  for (const auto& point : points) sum += accessor(point);
  return (float)(sum/points.size());
}

float MetricManager::median(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  std::vector<double> x;
  x.reserve(points.size());
  for (const auto& point : points) x.push_back(accessor(point));
  std::sort(x.begin(), x.end());
  return percentile(x, 50);
}

float MetricManager::sd(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  if (points.size() == 1) return NA_F32_RASTER;

  double sum = 0.0;
  for (const auto& point : points) sum += accessor(point);
  double mean = sum/points.size();

  sum = 0.0;
  for (const auto& point : points)
  {
    double value = accessor(point);
    sum += (value - mean) * (value - mean);
  }

  return (float)(std::sqrt(sum/(points.size()-1)));
}

float MetricManager::mode(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  std::unordered_map<double, int> registry;

  for (const auto& point : points)
  {
    double value = accessor(point);
    registry[value]++;
  }

  double mode = accessor(points[0]);
  int count = registry[mode];

  for (const auto& pair : registry)
  {
    if (pair.second > count)
    {
      mode = pair.first;
      count = pair.second;
    }
  }

  return (float)mode;
}

float MetricManager::cv(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  float avg = mean(accessor, points, param);
  float std = sd(accessor, points, param);
  if (avg == 0 || avg == NA_F32_RASTER || std == NA_F32_RASTER) return NA_F32_RASTER;
  return std/avg;
}

float MetricManager::sum(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  double sum = 0;
  for (const auto& point : points) sum += accessor(point);
  return (float)sum;
}

float MetricManager::count(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  return (float)points.size();
}


float MetricManager::percentile(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  std::vector<double> x;
  x.reserve(points.size());
  for (const auto& point : points) x.push_back(accessor(point));
  std::sort(x.begin(), x.end());
  return percentile(x, param);
}

float MetricManager::above(PointAttributeAccessor accessor, const PointCollection& points, float param) const
{
  float k = 0;
  for (const auto& point : points)
  {
    if (accessor(point) > param) k++;
  }
  return k/(float)points.size();
}

float MetricManager::get_metric(int index, float x, float y) const
{
  const StreamingMetric& f = streaming_operators[index];
  return (this->*f)(x,y);
}

float MetricManager::get_metric(int index, const PointCollection& points) const
{
  if (points.size() == 0) return default_value;
  return regular_operators[index].compute(points);
}

double MetricManager::percentile(const std::vector<double>& x, float p) const
{
  float rank = (p / 100.0f) * ((float)x.size() - 1) + 1;
  int lowerIndex = (int)(std::floor(rank)) - 1;
  int upperIndex = (int)(std::ceil(rank)) - 1;
  double lowerValue = x[lowerIndex];
  double upperValue = x[upperIndex];
  return lowerValue + (upperValue - lowerValue) * (rank - std::floor(rank));
}

int MetricManager::size() const
{
  return (streamable) ? (int)streaming_operators.size() : (int)regular_operators.size();
};

bool MetricManager::active() const
{
  return size() > 0;
}

float MetricManager::string_to_float(const std::string& s) const
{
  try
  {
    float height = std::stof(s);
    return height;
  }
  catch (std::exception& e)
  {
    return std::numeric_limits<float>::quiet_NaN();
  }
}


MetricManager::MetricManager()
{
  streamable = false;
  default_value = NA_F32_RASTER;
}

MetricManager::~MetricManager()
{
}

#include "Metrics.h"
#include "error.h"
#include "NA.h"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <limits>
#include <stdexcept>

bool Metrics::parse(const std::vector<std::string>& names)
{
  // Check if we have only streamable metrics
  streamable = true;
  for (const auto& name : names)
  {
    if (name != "max" && name != "min" && name != "count" && name != "z_max" && name != "z_min")
    {
      streamable = false;
      break;
    }
  }

  // If we have only streamable metrics
  if (streamable)
  {
    for (const auto& name : names)
    {
      if (name == "max" || name == "z_max")
        streaming_operators.push_back(&Metrics::pmax);
      else if (name == "min" || name == "z_min")
        streaming_operators.push_back(&Metrics::pmin);
      else if (name == "count")
        streaming_operators.push_back(&Metrics::pcount);
    }

    return true;
  }

  // Regular case including non streamable metrics
  try
  {
    for (const auto& name : names)
    {
      regular_operators.push_back(parse(name));
    }
  }
  catch(std::exception& e)
  {
    last_error = e.what();
    return false;
  }

  return true;
}

Metric Metrics::parse(const std::string& name)
{
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
    param = string2float(probs);

    if (std::isnan(param)) throw std::invalid_argument("Invalid parameter in: " + name);
    if (param < 0 || param > 100)  throw std::invalid_argument("Percentile out of range (0-100)");

    metric = metric[0];
  }
  else if (metric.substr(0,5) == "above")
  {
    std::string h = metric.substr(5);
    param = string2float(h);

    if (std::isnan(param)) throw std::invalid_argument("Invalid parameter in: " + name);

    metric = metric.substr(0,5);
  }

  auto accessor = attribute_functions.find(attribute);
  if (accessor == attribute_functions.end()) throw std::invalid_argument("Invalid attribute name: " + attribute);

  auto metric_function = metric_functions.find(metric);
  if (metric_function == metric_functions.end()) throw std::invalid_argument("Invalid metric name: " + metric);

  return Metric(metric_function->second, accessor->second, param);
}


void Metrics::add_point(const PointLAS& p)
{
  points.push_back(p);
}

void Metrics::reset()
{
  points.clear();
}

// streamable metrics
float Metrics::pmax  (float x, float y) const { if (x == NA_F32_RASTER) return y; return (x > y) ? x : y; }
float Metrics::pmin  (float x, float y) const { if (x == NA_F32_RASTER) return y; return (x < y) ? x : y; }
float Metrics::pcount(float x, float y) const { if (x == NA_F32_RASTER) return 1; return x+1; }

// batch metrics

float Metrics::min(PointAccessor accessor, const PointCloud& points, float param) const
{
  double min = std::numeric_limits<double>::max();
  for (const auto& point : points)
  {
    double val = accessor(point);
    if (min > val) min = val;
  }
  return min;
}

float Metrics::max(PointAccessor accessor, const PointCloud& points, float param) const
{
  double max = std::numeric_limits<double>::lowest();
  for (const auto& point : points)
  {
    double val = accessor(point);
    if (max < val) max = val;
  }
  return max;
}

float Metrics::mean(PointAccessor accessor, const PointCloud& points, float param) const
{
  double sum = 0.0;
  for (const auto& point : points) sum += accessor(point);
  return (float)(sum/points.size());
}

float Metrics::median(PointAccessor accessor, const PointCloud& points, float param) const
{
  std::vector<double> x;
  x.reserve(points.size());
  for (const auto& point : points) x.push_back(accessor(point));
  std::sort(x.begin(), x.end());
  return percentile(x, 50);
}

float Metrics::sd(PointAccessor accessor, const PointCloud& points, float param) const
{
  double sum = 0.0;
  for (const auto& point : points) sum += accessor(point);
  double mean = sum/points.size();

  for (const auto& point : points)
  {
    double value = accessor(point);
    sum += (value - mean) * (value - mean);
  }

  return (float)(std::sqrt(sum/(points.size()-1)));
}

float Metrics::mode(PointAccessor accessor, const PointCloud& points, float param) const
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

float Metrics::cv(PointAccessor accessor, const PointCloud& points, float param) const
{
  return sd(accessor, points, param)/mean(accessor, points, param);
}

float Metrics::sum(PointAccessor accessor, const PointCloud& points, float param) const
{
  double sum = 0;
  for (const auto& point : points) sum += accessor(point);
  return (float)sum;
}

float Metrics::count(PointAccessor accessor, const PointCloud& points, float param) const
{
  return (float)points.size();
}


float Metrics::percentile(PointAccessor accessor, const PointCloud& points, float param) const
{
  std::vector<double> x;
  x.reserve(points.size());
  for (const auto& point : points) x.push_back(accessor(point));
  std::sort(x.begin(), x.end());
  return percentile(x, param);
}

float Metrics::above(PointAccessor accessor, const PointCloud& points, float param) const
{
  float k = 0;
  for (const auto& point : points)
  {
    if (accessor(point) > param) k++;
  }
  return k/(float)points.size();
}

float Metrics::get_metric(int index, float x, float y) const
{
  const StreamingMetric& f = streaming_operators[index];
  return (this->*f)(x,y);
}

float Metrics::get_metric(int index) const
{
  if (points.size() == 0) return default_value;
  return regular_operators[index].compute(points);
}

double Metrics::percentile(const std::vector<double>& x, float p) const
{
  float rank = (p / 100.0f) * ((float)x.size() - 1) + 1;
  int lowerIndex = (int)(std::floor(rank)) - 1;
  int upperIndex = (int)(std::ceil(rank)) - 1;
  double lowerValue = x[lowerIndex];
  double upperValue = x[upperIndex];
  return lowerValue + (upperValue - lowerValue) * (rank - std::floor(rank));
}

int Metrics::size() const
{
  return (streamable) ? (int)streaming_operators.size() : (int)regular_operators.size();
};

float Metrics::string2float(const std::string& s) const
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


Metrics::Metrics()
{
  reset();
  streamable = false;
  default_value = NA_F32_RASTER;
}

Metrics::~Metrics()
{
}


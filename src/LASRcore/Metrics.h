#ifndef LASRMETRICS_H
#define LASRMETRICS_H

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

#include "PointSchema.h"

using PointCollection = std::vector<Point>;
using MetricComputation = std::function<float(AttributeAccessor&, const PointCollection&, float)>;

class MetricCalculator
{
public:
  MetricCalculator(MetricComputation computation, AttributeAccessor& accessor, float param) : computation(computation), accessor(accessor), param(param) {}
  float compute(const PointCollection& points) { return computation(accessor, points, param); }
  void set_param(float x) { param = x; }
  void reset() { accessor.reset(); };

private:
  MetricComputation computation;
  AttributeAccessor accessor;
  float param;
};

class MetricManager
{
public:
  MetricManager();
  ~MetricManager();
  bool parse(const std::vector<std::string>& names, bool support_streamable = true);
  int size() const;
  bool active() const;
  float get_metric(int index, const PointCollection& points);
  float get_metric(int index, float x, float y) const;
  const std::string& get_name(int index) { return names[index]; }
  float get_default_value() const { return default_value; }
  void set_default_value(float val) { default_value = val; }
  bool is_streamable() const { return streamable; }
  void reset();

private:
  double percentile(const std::vector<double>& x, float p) const;
  float string_to_float(const std::string& s) const;

  // Streamable metrics
  float pmax  (float x, float y) const;
  float pmin  (float x, float y) const;
  float pcount(float x, float y) const;

  // Non-streamable metrics
  float min(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float max(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float mean(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float median(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float sd(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float cv(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float sum(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float percentile(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float above(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float count(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float mode(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float moment(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float skewness(AttributeAccessor& accessor, const PointCollection& points, float param) const;
  float kurtosis(AttributeAccessor& accessor, const PointCollection& points, float param) const;

  float default_value;
  std::vector<std::string> names;

  // Predefined metrics such as z_max or z_min can be streamed. We use a pointer to simple functions.
  bool streamable;
  typedef float (MetricManager::*StreamingMetric)(float, float) const;
  std::vector<StreamingMetric> streaming_operators;

  // Regular metrics can't be streamed. We use a parser and a MetricCalculator object to handle the complexity of the system.
  MetricCalculator parse(const std::string& name);
  std::vector<MetricCalculator> regular_operators;

  // Map of string to metric functions
  std::unordered_map<std::string, MetricComputation> metric_functions = {
    {"max", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return max(accessor, points, param); }},
    {"min", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return min(accessor, points, param); }},
    {"mean", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return mean(accessor, points, param); }},
    {"median", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return median(accessor, points, param); }},
    {"sd", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return sd(accessor, points, param); }},
    {"cv", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return cv(accessor, points, param); }},
    {"sum", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return sum(accessor, points, param); }},
    {"above", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return above(accessor, points, param); }},
    {"mode", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return mode(accessor, points, param); }},
    {"count", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return count(accessor, points, param); }},
    {"p", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return percentile(accessor, points, param); }},
    {"skew", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return skewness(accessor, points, param); }},
    {"kurt", [this](AttributeAccessor& accessor, const PointCollection& points, float param) { return kurtosis(accessor, points, param); }},
  };
};


#endif
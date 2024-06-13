#ifndef LASRMETRICS_H
#define LASRMETRICS_H

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

#include "PointLAS.h"

using PointCollection = std::vector<PointLAS>;
using PointAttributeAccessor = std::function<double(const PointLAS&)>;
using MetricComputation = std::function<float(PointAttributeAccessor, const PointCollection&, float)>;

class MetricCalculator
{
public:
  MetricCalculator(MetricComputation computation, PointAttributeAccessor accessor, float param) : computation(computation), accessor(accessor), param(param) {}
  float compute(const PointCollection& points) const { return computation(accessor, points, param); }
  void set_param(float x) { param = x; }

private:
  MetricComputation computation;
  PointAttributeAccessor accessor;
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
  float get_metric(int index, const PointCollection& points) const;
  float get_metric(int index, float x, float y) const;
  const std::string& get_name(int index) { return names[index]; }
  float get_default_value() const { return default_value; }
  void set_default_value(float val) { default_value = val; }
  bool is_streamable() const { return streamable; }

private:
  double percentile(const std::vector<double>& x, float p) const;
  float string_to_float(const std::string& s) const;

  // Streamable metrics
  float pmax  (float x, float y) const;
  float pmin  (float x, float y) const;
  float pcount(float x, float y) const;

  // Non-streamable metrics
  float min(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float max(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float mean(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float median(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float sd(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float cv(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float sum(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float percentile(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float above(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float count(PointAttributeAccessor accessor, const PointCollection& points, float param) const;
  float mode(PointAttributeAccessor accessor, const PointCollection& points, float param) const;

  float default_value;
  std::vector<std::string> names;

  // Predefined metrics such as z_max or z_min can be streamed. We use a pointer to simple functions.
  bool streamable;
  typedef float (MetricManager::*StreamingMetric)(float, float) const;
  std::vector<StreamingMetric> streaming_operators;

  // Regular metrics can't be streamed. We use a parser and a MetricCalculator object to handle the complexity of the system.
  MetricCalculator parse(const std::string& name);
  std::vector<MetricCalculator> regular_operators;

  // Map of string to attribute accessors
  std::unordered_map<std::string, PointAttributeAccessor> attribute_functions = {
    {"x", [](const PointLAS& p) { return p.x; }},
    {"y", [](const PointLAS& p) { return p.y; }},
    {"z", [](const PointLAS& p) { return p.z; }},
    {"i", [](const PointLAS& p) { return p.intensity; }},
    {"r", [](const PointLAS& p) { return p.return_number; }},
    {"n", [](const PointLAS& p) { return p.number_of_returns; }},
    {"c", [](const PointLAS& p) { return p.classification; }},
    {"t", [](const PointLAS& p) { return p.gps_time; }},
    {"s", [](const PointLAS& p) { return p.synthetic_flag; }},
    {"k", [](const PointLAS& p) { return p.keypoint_flag; }},
    {"w", [](const PointLAS& p) { return p.withheld_flag; }},
    {"o", [](const PointLAS& p) { return p.overlap_flag; }},
    {"u", [](const PointLAS& p) { return p.user_data; }},
    {"p", [](const PointLAS& p) { return p.point_source_ID; }},
    {"e", [](const PointLAS& p) { return p.edge_of_flight_line; }},
    {"d", [](const PointLAS& p) { return p.scan_direction_flag; }},
    {"a", [](const PointLAS& p) { return p.scan_angle; }},
    {"R", [](const PointLAS& p) { return p.R; }},
    {"G", [](const PointLAS& p) { return p.G; }},
    {"B", [](const PointLAS& p) { return p.B; }},
    {"N", [](const PointLAS& p) { return p.NIR; }},

    // # nocov start
    {"intensity", [](const PointLAS& p) { return p.intensity; }},
    {"returnnumber", [](const PointLAS& p) { return p.return_number; }},
    {"numberofreturn", [](const PointLAS& p) { return p.number_of_returns; }},
    {"classification", [](const PointLAS& p) { return p.classification; }},
    {"gpstime", [](const PointLAS& p) { return p.gps_time; }},
    {"synthetic", [](const PointLAS& p) { return p.synthetic_flag; }},
    {"keypoint", [](const PointLAS& p) { return p.keypoint_flag; }},
    {"withheld", [](const PointLAS& p) { return p.withheld_flag; }},
    {"overlap", [](const PointLAS& p) { return p.overlap_flag; }},
    {"userdata", [](const PointLAS& p) { return p.user_data; }},
    {"pointsourceid", [](const PointLAS& p) { return p.point_source_ID; }},
    {"edgeofflightline", [](const PointLAS& p) { return p.edge_of_flight_line; }},
    {"scandirectionflag", [](const PointLAS& p) { return p.scan_direction_flag; }},
    {"angle", [](const PointLAS& p) { return p.scan_angle; }},
    {"scanangle", [](const PointLAS& p) { return p.scan_angle; }},
    {"red", [](const PointLAS& p) { return p.R; }},
    {"green", [](const PointLAS& p) { return p.G; }},
    {"blue", [](const PointLAS& p) { return p.B; }},
    {"nir", [](const PointLAS& p) { return p.NIR; }}
    // # nocov end
  };

  // Map of string to metric functions
  std::unordered_map<std::string, MetricComputation> metric_functions = {
    {"max", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return max(accessor, points, param); }},
    {"min", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return min(accessor, points, param); }},
    {"mean", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return mean(accessor, points, param); }},
    {"median", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return median(accessor, points, param); }},
    {"sd", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return sd(accessor, points, param); }},
    {"cv", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return cv(accessor, points, param); }},
    {"sum", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return sum(accessor, points, param); }},
    {"above", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return above(accessor, points, param); }},
    {"mode", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return mode(accessor, points, param); }},
    {"count", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return count(accessor, points, param); }},
    {"p", [this](PointAttributeAccessor accessor, const PointCollection& points, float param) { return percentile(accessor, points, param); }}
  };
};


#endif
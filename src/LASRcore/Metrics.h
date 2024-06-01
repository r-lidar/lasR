#ifndef LASRMETRICS_H
#define LASRMETRICS_H

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

#include "Shape.h"

using PointCloud = std::vector<PointLAS>;
using PointAccessor = std::function<double(const PointLAS&)>;
using MetricFunction = std::function<float(PointAccessor, const PointCloud&, float)>;

class Metric
{
public:
  Metric(MetricFunction metric, PointAccessor accessor, float param): metric(metric), accessor(accessor), param(param) {}
  float compute(const PointCloud& points) const { return metric(accessor, points, param); };
  void set_param(float x) { param = x; };

private:
  MetricFunction metric;
  PointAccessor accessor;
  float param;
};

class Metrics
{
public:
  Metrics();
  ~Metrics();
  void add_point(const PointLAS& p);
  bool parse(const std::vector<std::string>& names);
  void reset();
  int size() const;
  float get_metric(int index) const;
  float get_metric(int index, float x, float y) const;
  float get_default_value() const { return default_value; };
  void set_default_value(float val) { default_value = val; };
  bool is_streamable() const { return streamable; }

private:
  double percentile(const std::vector<double>& x, float p) const;

  float string2float(const std::string& s) const;

  // streamable metrics
  float pmax  (float x, float y) const;
  float pmin  (float x, float y) const;
  float pcount(float x, float y) const;

  // non streamable metrics
  float min(PointAccessor accessor, const PointCloud& points, float param) const;
  float max(PointAccessor accessor, const PointCloud& points, float param) const;
  float mean(PointAccessor accessor, const PointCloud& points, float param) const;
  float median(PointAccessor accessor, const PointCloud& points, float param) const;
  float sd(PointAccessor accessor, const PointCloud& points, float param) const;
  float cv(PointAccessor accessor, const PointCloud& points, float param) const;
  float sum(PointAccessor accessor, const PointCloud& points, float param) const;
  float percentile(PointAccessor accessor, const PointCloud& points, float param) const;
  float above(PointAccessor accessor, const PointCloud& points, float param) const;
  float count(PointAccessor accessor, const PointCloud& points, float param) const;
  float mode(PointAccessor accessor, const PointCloud& points, float param) const;

  float default_value;
  PointCloud points;

  // Some predefined metrics such as z_max or z_min can be streamed. We use a pointer to simple functions.
  bool streamable;
  typedef float (Metrics::*StreamingMetric)(float, float) const;
  std::vector<StreamingMetric> streaming_operators;

  // Regular metrics can't be streamed. We use a parser and a Metrics object to handle the complexity of the
  // system.
  Metric parse(const std::string& name);
  std::vector<Metric> regular_operators;

  // Map of string to attribute accessors
  std::unordered_map<std::string, PointAccessor> attribute_functions = {
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

    {"intensity", [](const PointLAS& p) { return p.intensity; }},
    {"returnnumber", [](const PointLAS& p) { return p.return_number; }},
    {"numberofreturn", [](const PointLAS& p) { return p.number_of_returns; }},
    {"cclassification", [](const PointLAS& p) { return p.classification; }},
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
    {"gren", [](const PointLAS& p) { return p.G; }},
    {"blue", [](const PointLAS& p) { return p.B; }},
    {"nir", [](const PointLAS& p) { return p.NIR; }}
  };

  // Map of string to metric functions
  std::unordered_map<std::string, MetricFunction> metric_functions = {
    {"max", [this](PointAccessor accessor, const PointCloud& points, float param) { return max(accessor, points, param); }},
    {"min", [this](PointAccessor accessor, const PointCloud& points, float param) { return min(accessor, points, param); }},
    {"mean", [this](PointAccessor accessor, const PointCloud& points, float param) { return mean(accessor, points, param); }},
    {"median", [this](PointAccessor accessor, const PointCloud& points, float param) { return median(accessor, points, param); }},
    {"sd", [this](PointAccessor accessor, const PointCloud& points, float param) { return sd(accessor, points, param); }},
    {"cv", [this](PointAccessor accessor, const PointCloud& points, float param) { return cv(accessor, points, param); }},
    {"sum", [this](PointAccessor accessor, const PointCloud& points, float param) { return sum(accessor, points, param); }},
    {"above", [this](PointAccessor accessor, const PointCloud& points, float param) { return above(accessor, points, param); }},
    {"mode", [this](PointAccessor accessor, const PointCloud& points, float param) { return mode(accessor, points, param); }},
    {"count", [this](PointAccessor accessor, const PointCloud& points, float param) { return count(accessor, points, param); }},
    {"p", [this](PointAccessor accessor, const PointCloud& points, float param) { return percentile(accessor, points, param); }}
  };
};

#endif
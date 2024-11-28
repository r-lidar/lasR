#ifndef LASRMETRICS_H
#define LASRMETRICS_H

#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

#include "PointSchema.h"

using PointCollection = std::vector<Point>;
using MetricComputation = std::function<float(AttributeHandler&, const PointCollection&, float)>;

class MetricCalculator
{
public:
  MetricCalculator(MetricComputation computation, AttributeHandler& accessor, float param) : computation(computation), accessor(accessor), param(param) {}
  float compute(const PointCollection& points) { return computation(accessor, points, param); }
  void set_param(float x) { param = x; }
  void reset() { accessor.reset(); };

private:
  MetricComputation computation;
  AttributeHandler accessor;
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
  float min(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float max(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float mean(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float median(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float sd(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float cv(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float sum(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float percentile(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float above(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float count(AttributeHandler& accessor, const PointCollection& points, float param) const;
  float mode(AttributeHandler& accessor, const PointCollection& points, float param) const;

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
  /*std::unordered_map<std::string, AttributeHandler> attribute_functions = {
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
  };*/

  // Map of string to metric functions
  std::unordered_map<std::string, MetricComputation> metric_functions = {
    {"max", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return max(accessor, points, param); }},
    {"min", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return min(accessor, points, param); }},
    {"mean", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return mean(accessor, points, param); }},
    {"median", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return median(accessor, points, param); }},
    {"sd", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return sd(accessor, points, param); }},
    {"cv", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return cv(accessor, points, param); }},
    {"sum", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return sum(accessor, points, param); }},
    {"above", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return above(accessor, points, param); }},
    {"mode", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return mode(accessor, points, param); }},
    {"count", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return count(accessor, points, param); }},
    {"p", [this](AttributeHandler& accessor, const PointCollection& points, float param) { return percentile(accessor, points, param); }}
  };
};


#endif
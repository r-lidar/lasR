#ifndef LASRMETRICS_H
#define LASRMETRICS_H

#include <vector>
#include <string>

#include "macros.h"

#include "laszip.hpp"
#include "laspoint.hpp"

class LASRmetrics
{
public:
  LASRmetrics();
  ~LASRmetrics();
  bool parse(const std::vector<std::string>& names);
  void add_point(const LASpoint* p);
  void reset();
  int size() const { return (streamable) ? streaming_operators.size() : regular_operators.size(); };
  float get_metric(int index);
  float get_metric(int index, float x, float y);
  bool is_streamable() const { return streamable; }

private:
  void clean();

  // streamable metrics
  float pmax  (float x, float y) const;
  float pmin  (float x, float y) const;
  float pcount(float x, float y) const;

  // batch metrics
  float zmax() const;
  float zmin() const;
  float zmean() const;
  float zmedian() const;
  float imax() const;
  float imin() const;
  float imean() const;
  float imedian() const;
  float count() const;

  // data storage
  std::vector<double> i;
  std::vector<double> z;
  double isum;
  double zsum;
  int n;

  // internal stage
  bool sorted;
  bool streamable;

  // metrics to apply
  typedef float (LASRmetrics::*RegularMetric)() const;
  typedef float (LASRmetrics::*StreamingMetric)(float x, float y) const;
  std::vector<StreamingMetric> streaming_operators;
  std::vector<RegularMetric> regular_operators;
};

#endif
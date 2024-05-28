#ifndef LASRMETRICS_H
#define LASRMETRICS_H

#include <vector>
#include <string>

#include "Shape.h"

class LASRmetrics
{
public:
  LASRmetrics();
  ~LASRmetrics();
  bool parse(const std::vector<std::string>& names);
  void add_point(const PointLAS& p);
  void reset();
  int size() const;
  float get_metric(int index);
  float get_metric(int index, float x, float y);
  float get_default_value() { return default_value; };
  void set_default_value(float val) { default_value = val; };
  bool is_streamable() const { return streamable; }

private:
  void clean();
  float percentile(const std::vector<double>& x, float p) const;

  float string2float(const std::string& s);

  // streamable metrics
  float pmax  (float x, float y) const;
  float pmin  (float x, float y) const;
  float pcount(float x, float y) const;

  // batch metrics
  float zmax(float) const;
  float zmin(float) const;
  float zmean(float) const;
  float zmedian(float) const;
  float zsd(float) const;
  float zcv(float) const;
  float zpx(float) const;
  float zabove(float) const;
  float imax(float) const;
  float imin(float) const;
  float imean(float) const;
  float imedian(float) const;
  float isd(float) const;
  float icv(float) const;
  float ipx(float) const;
  float count(float) const;

  // Some metrics can take a parameter such as zpx
  std::vector<float> param;

  // data storage
  std::vector<double> i;
  std::vector<double> z;
  double isum;
  double zsum;
  int n;

  // internal state
  bool sorted;
  bool streamable;

  // metrics to apply
  typedef float (LASRmetrics::*RegularMetric)(float) const;
  typedef float (LASRmetrics::*StreamingMetric)(float, float) const;
  std::vector<StreamingMetric> streaming_operators;
  std::vector<RegularMetric> regular_operators;

  float default_value;
};

#endif
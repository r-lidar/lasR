#include "Metrics.h"
#include "error.h"
#include "NA.h"

#include <cmath>
#include <numeric>
#include <algorithm>

bool LASRmetrics::parse(const std::vector<std::string>& names)
{
  // Check if we have only streamable metrics
  streamable = true;
  for (const auto name : names)
  {
    if (name != "max" && name != "min" && name != "count" && name != "zmax" && name != "zmin")
    {
      streamable = false;
      break;
    }
  }

  // If we have only streamable metrics
  if (streamable)
  {
    for (const auto name : names)
    {
      if (name == "max" | name== "zmax")
        streaming_operators.push_back(&LASRmetrics::pmax);
      else if (name == "min" | name == "zmin")
        streaming_operators.push_back(&LASRmetrics::pmin);
      else if (name == "count")
        streaming_operators.push_back(&LASRmetrics::pcount);
    }

    return true;
  }

  // Regular case including non streamable metrics
  for (const auto name : names)
  {
    if (name[0] == 'z')
    {
      if (name == "zmax")
        regular_operators.push_back(&LASRmetrics::zmax);
      else if (name == "zmin")
        regular_operators.push_back(&LASRmetrics::zmin);
      else if (name == "zmean")
        regular_operators.push_back(&LASRmetrics::zmean);
      else if (name == "zmedian")
        regular_operators.push_back(&LASRmetrics::zmedian);
      else if (name == "zsd")
        regular_operators.push_back(&LASRmetrics::zsd);
      else if (name == "zcv")
        regular_operators.push_back(&LASRmetrics::zcv);
      else
      {
        last_error = "metric " + name + "not recognized";
        return false;
      }
    }
    else if (name[0] == 'i')
    {
      if (name == "imax")
        regular_operators.push_back(&LASRmetrics::imax);
      else if (name == "imin")
        regular_operators.push_back(&LASRmetrics::imin);
      else if (name == "imean")
        regular_operators.push_back(&LASRmetrics::imean);
      else if (name == "imedian")
        regular_operators.push_back(&LASRmetrics::imedian);
      else if (name == "isd")
        regular_operators.push_back(&LASRmetrics::isd);
      else if (name == "icv")
        regular_operators.push_back(&LASRmetrics::icv);
      else
      {
        last_error = "metric " + name + "not recognized";
        return false;
      }
    }
    else if (name == "count")
      regular_operators.push_back(&LASRmetrics::count);
    else
    {
      last_error = "metric " + name + "not recognized";
      return false;
    }
  }

  return true;
}

void LASRmetrics::add_point(const LASpoint* p)
{
  z.push_back(p->get_z());
  i.push_back(p->get_intensity());
  zsum += p->get_z();
  isum += p->get_intensity();
  n++;
}


void LASRmetrics::reset()
{
  z.clear();
  i.clear();
  zsum = 0;
  isum = 0;
  n = 0;
  sorted = false;
}

// streamable metrics
float LASRmetrics::pmax  (float x, float y) const { if (x == NA_F32_RASTER) return y; return (x > y) ? x : y; }
float LASRmetrics::pmin  (float x, float y) const { if (x == NA_F32_RASTER) return y; return (x < y) ? x : y; }
float LASRmetrics::pcount(float x, float y) const { if (x == NA_F32_RASTER) return 1; return x+1; }

// batch metrics
float LASRmetrics::zmax() const { return (float)z[n-1]; }
float LASRmetrics::zmin() const { return (float)z[0]; }
float LASRmetrics::zmean() const { return (float)zsum/n; }
float LASRmetrics::zmedian() const { return (n % 2 == 0) ? (float)((z[n/2 - 1] + z[n/2])/2) : (float)z[n/2]; }
float LASRmetrics::zsd() const { float m = zmean(); float sd = 0; for(size_t j = 0; j < n; ++j) { sd += pow(z[j]-m, 2); } return std::sqrt(sd)/n; }
float LASRmetrics::zcv() const { return zsd()/zmean(); }
float LASRmetrics::imax() const { return (float)i[i.size()-1]; }
float LASRmetrics::imin() const { return (float)i[0]; }
float LASRmetrics::imean() const { return (float)isum/n; }
float LASRmetrics::imedian() const { return (n % 2 == 0) ? (float)((i[n/2 - 1] + i[n/2])/2) : (float)i[n/2]; }
float LASRmetrics::isd() const { float m = imean(); float sd = 0; for(size_t j = 0; j < n; ++j) { sd += pow(i[j]-m, 2); } return std::sqrt(sd)/n; }
float LASRmetrics::icv() const { return isd()/imean(); }

float LASRmetrics::count() const { return (float)n; }

float LASRmetrics::get_metric(int index, float x, float y)
{
  StreamingMetric& f = streaming_operators[index];
  return (this->*f)(x,y);
}

float LASRmetrics::get_metric(int index)
{
  if (!sorted)
  {
    std::sort(z.begin(), z.end());
    std::sort(i.begin(), i.end());
    sorted = true;
  }

  if (n == 0) return NA_F32_RASTER;

  RegularMetric& f = regular_operators[index];
  return (this->*f)();
}

/*float LASRmetrics::percentile(const std::vector<double>& x, double p) const
{
  // x is already sorted
  double rank = (p / 100.0) * ((double)n - 1) + 1;

  // Check if rank is an integer
  if (std::floor(rank) == rank)
  {
    return x[(int)(rank) - 1];
  }
  else
  {
    // Interpolating when rank is not an integer
    int lowerIndex = (int)(std::floor(rank)) - 1;
    int upperIndex = (int)(std::ceil(rank)) - 1;
    double lowerValue = x[lowerIndex];
    double upperValue = x[upperIndex];
    return lowerValue + (upperValue - lowerValue) * (rank - std::floor(rank));
  }
}*/


LASRmetrics::LASRmetrics()
{
  reset();
  streamable = false;
}

LASRmetrics::~LASRmetrics()
{
}

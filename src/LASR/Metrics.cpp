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
  for (const auto& name : names)
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
    for (const auto& name : names)
    {
      if (name == "max" || name== "zmax")
        streaming_operators.push_back(&LASRmetrics::pmax);
      else if (name == "min" || name == "zmin")
        streaming_operators.push_back(&LASRmetrics::pmin);
      else if (name == "count")
        streaming_operators.push_back(&LASRmetrics::pcount);
    }

    return true;
  }

  // Regular case including non streamable metrics
  for (const auto& name : names)
  {
    if (name[0] == 'z')
    {
      if (name == "zmax")
      {
        regular_operators.push_back(&LASRmetrics::zmax);
        param.push_back(0);
      }
      else if (name == "zmin")
      {
        regular_operators.push_back(&LASRmetrics::zmin);
        param.push_back(0);
      }
      else if (name == "zmean")
      {
        regular_operators.push_back(&LASRmetrics::zmean);
        param.push_back(0);
      }
      else if (name == "zmedian")
      {
        regular_operators.push_back(&LASRmetrics::zmedian);
        param.push_back(0);
      }
      else if (name == "zsd")
      {
        regular_operators.push_back(&LASRmetrics::zsd);
        param.push_back(0);
      }
      else if (name == "zcv")
      {
        regular_operators.push_back(&LASRmetrics::zcv);
        param.push_back(0);
      }
      else if (name[1] == 'p')
      {
        std::string probs = name.substr(2);
        int number = std::stoi(probs);
        if (number < 0 || number > 100)
        {
          last_error = "Percentile out of range (0-100)";
          return false;
        }
        regular_operators.push_back(&LASRmetrics::zpx);
        param.push_back(number);
      }
      else
      {
        last_error = "metric " + name + "not recognized";
        return false;
      }
    }
    else if (name[0] == 'i')
    {
      if (name == "imax")
      {
        regular_operators.push_back(&LASRmetrics::imax);
        param.push_back(0);
      }
      else if (name == "imin")
      {
        regular_operators.push_back(&LASRmetrics::imin);
        param.push_back(0);
      }
      else if (name == "imean")
      {
        regular_operators.push_back(&LASRmetrics::imean);
        param.push_back(0);
      }
      else if (name == "imedian")
      {
        regular_operators.push_back(&LASRmetrics::imedian);
        param.push_back(0);
      }
      else if (name == "isd")
      {
        regular_operators.push_back(&LASRmetrics::isd);
        param.push_back(0);
      }
      else if (name == "icv")
      {
        regular_operators.push_back(&LASRmetrics::icv);
        param.push_back(0);
      }
      else if (name[1] == 'p')
      {
        std::string probs = name.substr(2);
        int number = std::stoi(probs);
        if (number < 0 || number > 100)
        {
          last_error = "Percentile out of range (0-100)";
          return false;
        }
        regular_operators.push_back(&LASRmetrics::ipx);
        param.push_back(number);
      }
      else
      {
        last_error = "metric " + name + "not recognized";
        return false;
      }
    }
    else if (name == "count")
    {
      regular_operators.push_back(&LASRmetrics::count);
      param.push_back(0);
    }
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
float LASRmetrics::zmax(float p) const { return (float)z[n-1]; }
float LASRmetrics::zmin(float p) const { return (float)z[0]; }
float LASRmetrics::zmean(float p) const { return (float)zsum/n; }
float LASRmetrics::zmedian(float p) const { return (n % 2 == 0) ? (float)((z[n/2 - 1] + z[n/2])/2) : (float)z[n/2]; }
float LASRmetrics::zsd(float p) const { float m = zmean(0); float sd = 0; for(size_t j = 0; j < n; ++j) { sd += pow(z[j]-m, 2); } return std::sqrt(sd)/n; }
float LASRmetrics::zcv(float p) const { return zsd(0)/zmean(0); }
float LASRmetrics::zpx(float p) const { return percentile(z, p); }
float LASRmetrics::imax(float p) const { return (float)i[i.size()-1]; }
float LASRmetrics::imin(float p) const { return (float)i[0]; }
float LASRmetrics::imean(float p) const { return (float)isum/n; }
float LASRmetrics::imedian(float p) const { return (n % 2 == 0) ? (float)((i[n/2 - 1] + i[n/2])/2) : (float)i[n/2]; }
float LASRmetrics::isd(float p) const { float m = imean(0); float sd = 0; for(size_t j = 0; j < n; ++j) { sd += pow(i[j]-m, 2); } return std::sqrt(sd)/n; }
float LASRmetrics::icv(float p) const { return isd(0)/imean(0); }
float LASRmetrics::ipx(float p) const { return percentile(i, p); }
float LASRmetrics::count(float p) const { return (float)n; }

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
  return (this->*f)(param[index]);
}

float LASRmetrics::percentile(const std::vector<double>& x, float p) const
{
  float rank = (p / 100.0f) * ((float)n - 1) + 1;
  int lowerIndex = (int)(std::floor(rank)) - 1;
  int upperIndex = (int)(std::ceil(rank)) - 1;
  double lowerValue = x[lowerIndex];
  double upperValue = x[upperIndex];
  return lowerValue + (upperValue - lowerValue) * (rank - std::floor(rank));
}

int LASRmetrics::size() const
{
  return (streamable) ? (int)streaming_operators.size() : (int)regular_operators.size();
};

LASRmetrics::LASRmetrics()
{
  reset();
  streamable = false;
}

LASRmetrics::~LASRmetrics()
{
}

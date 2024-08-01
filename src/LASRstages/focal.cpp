#include "focal.h"

#include <algorithm>
#include <numeric>

LASRfocal::LASRfocal(float size, const std::string& method, Stage* stage)
{
  set_connection(stage);
  this->size = size;

  // Initialize the output raster from input raster
  StageRaster* p = dynamic_cast<StageRaster*>(stage);
  if (p)
    raster = Raster(p->get_raster());
  else
    throw std::string("Invalid connection with stage ") + stage->get_uid();

  // Select the operation based on the string
  if (method == "mean")
    operation = mean;
  else if (method == "median")
    operation = median;
  else if (method == "sum")
    operation = sum;
  else if (method == "min")
    operation = min;
  else if (method == "max")
    operation = max;
  else
    throw std::string("Unknown operation: ") + method;
}

bool LASRfocal::process()
{
  if (connections.empty())
  {
    last_error = "Unitialized pointer to Stage"; // # nocov
    return false; // # nocov
  }

  // 'connections' contains a single stage that is supposed to be a raster stage
  // This is the only supported stage as of aug 2024
  auto it = connections.begin();
  StageRaster* p = dynamic_cast<StageRaster*>(it->second);
  if (!p)
  {
    last_error = "Invalid pointer dynamic cast. Expecting a pointer to StageRaster"; // # nocov
    return false; // # nocov
  }

  const Raster& rin = p->get_raster();
  if (!raster.copy_data(rin)) return false;
  raster.focal(size, operation);

  return true;
}

float LASRfocal::mean(std::vector<float>& vals)
{
  return std::accumulate(vals.begin(), vals.end(), 0.0f) / vals.size();
}

float LASRfocal::median(std::vector<float>& vals)
{
  std::sort(vals.begin(), vals.end());

  if (vals.size() % 2 == 0)
    return (vals[vals.size() / 2 - 1] + vals[vals.size() / 2]) / 2;
  else
    return vals[vals.size() / 2];
}

float LASRfocal::sum(std::vector<float>& vals)
{
  return std::accumulate(vals.begin(), vals.end(), 0.0f);
}

float LASRfocal::min(std::vector<float>& vals)
{
  return *std::min_element(vals.begin(), vals.end());
}

float LASRfocal::max(std::vector<float>& vals)
{
  return *std::max_element(vals.begin(), vals.end());
}
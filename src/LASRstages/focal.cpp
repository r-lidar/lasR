#include "focal.h"

#include <algorithm>
#include <numeric>

bool LASRfocal::set_parameters(const nlohmann::json& stage)
{
  size = stage.at("size");

  if (size < 0)
  {
    last_error = "size must be positive";
    return false;
  }

  std::string method = stage.value("fun", "mean");

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
  {
    last_error = std::string("Unknown operation: ") + method;
    return false;
  }

  auto it = connections.begin();
  StageRaster* p = dynamic_cast<StageRaster*>(it->second);
  raster = Raster(p->get_raster());
  return true;
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

bool LASRfocal::connect(const std::list<std::unique_ptr<Stage>>& pipeline, const std::string& uid)
{
  Stage* s = search_connection(pipeline, uid);

  if (s == nullptr) return false;

  StageRaster* p = dynamic_cast<StageRaster*>(s);

  if (p)
    set_connection(p);
  else
  {
    last_error = "Incompatible stage combination for 'focal'"; // # nocov
    return false; // # nocov
  }

  return true;
}
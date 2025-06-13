#include <fstream>
#include <iostream>

#include "api.h"

namespace api
{

Pipeline::Pipeline(const Stage& s)
{
  stages.push_back(s);
}

Pipeline Pipeline::operator+(const Stage& s) const
{
  Pipeline result = Pipeline(*this);
  result.stages.push_back(s);
  return result;
}

Pipeline Pipeline::operator+(const Pipeline& other) const
{
  Pipeline result = Pipeline(*this);
  result.stages.insert(result.stages.end(), other.stages.begin(), other.stages.end());
  return result;
}

Pipeline& Pipeline::operator+=(const Stage& s)
{
  stages.push_back(s);
  return *this;
}

Pipeline& Pipeline::operator+=(const Pipeline& other)
{
  stages.insert(stages.end(), other.stages.begin(), other.stages.end());
  return *this;
}

std::string Pipeline::to_string() const
{
  std::string out;
  for (const auto& s : stages)
    out += s.to_string();
  out +=  "-----------\n";
  return out;
}

bool Pipeline::has_reader() const
{
  for (const auto& stage : stages)
  {
    if (stage.get_name() == "reader")
      return true;
  }

  return false;
}

std::string Pipeline::write_json(const std::string& path) const
{
  std::filesystem::path temp_file;

  if (path == "")
  {
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path();  // platform independent
    temp_file = temp_dir / "pipeline.json";
  }
  else
  {
    temp_file = path;
  }

  std::ofstream file(temp_file);
  if (!file) throw std::runtime_error("Failed to open file: " + temp_file.string());

  file << generate_json();
  file.close();

  return temp_file;
}

nlohmann::json Pipeline::generate_json() const
{
  if (files.size() == 0)
    throw std::invalid_argument("No file associated to this pipeline. Cannot generate a valid JSON.");

  // We crate a temporary copy because we are adding stages (reader, build_catalog)
  // on the fly and we can this object to be const.
  Pipeline p(*this);

  if (!p.has_reader())
  {
    Stage reader("reader");
    p.stages.push_front(reader);
  }

  Stage catalog("build_catalog");
  catalog.set("files", files);
  catalog.set("buffer", opt_buffer);
  catalog.set("chunk", opt_chunk);
  p.stages.push_front(catalog);

  nlohmann::json j;

  // Processing otions
  j["processing"]["ncore"]     = opt_ncores;
  j["processing"]["strategy"]  = opt_strategy;
  j["processing"]["buffer"]    = opt_buffer;
  j["processing"]["progress"]  = opt_progress;
  j["processing"]["chunk"]     = opt_chunk;
  j["processing"]["verbose"]   = opt_verbose;
  j["processing"]["profiling"] = opt_profiling_file;

  // Pipeline
  j["pipeline"] = nlohmann::json::array();
  for (const auto& stage : p.stages)
    j["pipeline"].push_back(stage.to_json());

  //std::cout << std::setw(2) << j << std::endl;

  return j;
}

}
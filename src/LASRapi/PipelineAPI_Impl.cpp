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

Pipeline Pipeline::operator[](std::size_t index) const
{
  if (index < 0 || index >= stages.size())
    throw std::out_of_range("Stage index out of range");

  auto it = stages.begin();
  std::advance(it, index);

  const Stage& s = *it;
  return Pipeline(s);
}

void Pipeline::set_sequential_strategy()
{
  opt_ncores = {1, 0};
  opt_strategy = "sequential";
}

void Pipeline::set_concurrent_points_strategy(int ncores)
{
  opt_ncores = {ncores, 0};
  opt_strategy = "concurrent-points";
}

void Pipeline::set_concurrent_files_strategy(int ncores)
{
  opt_ncores = {ncores, 0};
  opt_strategy = "concurrent-files";
}

void Pipeline::set_nested_strategy(int ncores1, int ncores2)
{
  opt_ncores = {ncores1, ncores2};
  opt_strategy = "nested";
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

bool Pipeline::has_catalog() const
{
  for (const auto& stage : stages)
  {
    if (stage.get_name() == "build_catalog")
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

  file << generate_json().dump(2);
  file.close();

  return temp_file.string();
}

nlohmann::json Pipeline::generate_json() const
{
  // Create a temporary copy of the pipeline since this function is `const`
  // and we may need to insert additional stages (e.g., "reader", "build_catalog").
  Pipeline p(*this);

  // If a 'build_catalog' stage exists without a preceding 'reader' stage, this is invalid.
  // This situation can only occur when 'build_catalog' is manually inserted—e.g., by the R API
  // to support LAS objects or external pointers. Such a pipeline is inconsistent.
  if (p.has_catalog() && !p.has_reader())
    throw std::invalid_argument("A pipeline cannot contain a 'build_catalog' stage without a 'reader' stage");

  // If the pipeline is missing the mandatory 'reader' stage, insert it.
  if (!p.has_reader())
  {
    Stage reader("reader");
    p.stages.push_front(reader);
  }

  // If files were specified, we need to ensure the presence of a 'build_catalog' stage.
  // If not, assume generate_json() was called without set_file(), which is allowed
  // for parsing or inspection purposes, though execution would fail.
  if (!files.empty())
  {
    // Normally, the 'build_catalog' stage should not exist yet and is added here.
    // However, the R API may inject a non-standard 'build_catalog' to support
    // external data sources.
    if (!has_catalog())
    {
      Stage catalog("build_catalog");
      catalog.set("files", files);
      catalog.set("buffer", opt_buffer);
      catalog.set("chunk", opt_chunk);
      if (!opt_noprocess.empty())
        catalog.set("noprocess", opt_noprocess);
      p.stages.push_front(catalog);
    }
    else
    {
      // Update the existing 'build_catalog' stage with current options.
      auto it = p.stages.begin();
      it->set("buffer", opt_buffer);
      it->set("chunk", opt_chunk);
    }
  }

  // Construct the output JSON
  nlohmann::json j;

  // Add global processing options
  j["processing"]["ncores"]     = opt_ncores;
  j["processing"]["strategy"]  = opt_strategy;
  j["processing"]["buffer"]    = opt_buffer;
  j["processing"]["progress"]  = opt_progress;
  j["processing"]["chunk"]     = opt_chunk;
  j["processing"]["verbose"]   = opt_verbose;
  j["processing"]["profiling"] = opt_profiling_file;

  // Serialize the pipeline stages
  j["pipeline"] = nlohmann::json::array();
  for (const auto& stage : p.stages)
    j["pipeline"].push_back(stage.to_json());

  return j;
}


void Pipeline::set_noprocess(const std::vector<bool>& b)
{
  if ((b.size() > 0) && (files.size() != b.size()))
    throw std::invalid_argument("'noprocess' and 'on' have different length");

  this->opt_noprocess = b;
}


}
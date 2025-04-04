#ifdef USING_PYTHON

#include <string>
#include <iostream>
#include <fstream>
#include "LASRcore/RAM.h"
#include "LASRcore/openmp.h"
#include "LASRcore/error.h"
#include "LASRcore/print.h"
#include "LASRcore/pipeline.h"
#include "LASRcore/DrawflowParser.h"
#include "nlohmann/json.hpp"

// Forward declaration for process function
extern bool process(const std::string& config_file);

/**
 * Command-line entry point for the Python module
 * Not directly used by the Python bindings but can be useful for testing
 */
int main_python(int argc, char* argv[])
{
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <config_file.json>" << std::endl;
    return 1;
  }
  
  std::string file = argv[1];
  return process(file) ? 0 : 1;
}

/**
 * Get information about a pipeline configuration file
 * 
 * @param config_file Path to the JSON configuration file
 * @return JSON string containing pipeline information
 */
std::string get_pipeline_info(const std::string& config_file) {
  try {
    std::ifstream fjson(config_file);
    if (!fjson.is_open()) {
      last_error = "Could not open the json file containing the pipeline";
      throw last_error;
    }
    
    nlohmann::json json;
    fjson >> json;
    
    // The json file is maybe a file produced by Drawflow. It must be converted into something
    // understandable by lasR
    if (json.contains("drawflow")) {
      try {
        json = DrawflowParser::parse(json);
      } catch (std::exception& e) {
        throw std::string(e.what());
      }
    }
    
    nlohmann::json json_pipeline = json["pipeline"];
    
    Pipeline pipeline;
    if (!pipeline.parse(json_pipeline)) {
      throw last_error;
    }
    
    // Create JSON object with pipeline information
    nlohmann::json pipeline_info;
    pipeline_info["streamable"] = pipeline.is_streamable();
    pipeline_info["read_points"] = pipeline.need_points();
    pipeline_info["buffer"] = pipeline.need_buffer();
    pipeline_info["parallelizable"] = pipeline.is_parallelizable();
    pipeline_info["parallelized"] = pipeline.is_parallelized();
    pipeline_info["R_API"] = pipeline.use_rcapi();
    
    return pipeline_info.dump();
  } catch (std::string e) {
    last_error = e;
    eprint("%s", e.c_str());
    return "{}";
  } catch (...) {
    last_error = "c++ exception (unknown reason)";
    eprint("c++ exception (unknown reason)");
    return "{}";
  }
}

#endif 
#include "DrawflowParser.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <exception>

#include <iostream>

nlohmann::json DrawflowParser::parse(nlohmann::json json)
{
  auto drawflow = json["drawflow"];

  if (!drawflow.is_object())
    throw std::runtime_error("'drawflow' node is missing or is not an object");

  if (!drawflow.contains("Home") || !drawflow["Home"].is_object())
    throw std::runtime_error("'Home' node is missing or is not an object");

  auto home = drawflow["Home"];

  if (!home.contains("data") || !home["data"].is_object())
    throw std::runtime_error("data' node is missing or is not an object");

  auto data = home["data"];

  // Iterate through the data object to find the processing_options node
  // Then clean it
  // ===================================================================
  nlohmann::json processing_options;
  for (auto it = data.begin(); it != data.end(); it++)
  {
    if (it.value().contains("name") && it.value()["name"] == "processing_options")
    {
      processing_options = it.value();
      it = data.erase(it);
      break;
    }
  }

  if (processing_options.empty())
    throw std::runtime_error("Error: no 'Processing options' node.");

  if (!processing_options.contains("data"))
    throw std::runtime_error("Internal error 'processing_options' does not have any data");

  if (!processing_options["data"].contains("files"))
    throw std::runtime_error("Error in Processing Options: 'files' is missing");

  nlohmann::json processing_json = nlohmann::json::object();
  for (const auto& [key, value] : processing_options["data"].items())
  {
    processing_json[key] = convert_if_numeric(value);
  }

  //std::cout << std::setw(2) << processing_json << std::endl;

  // Parse and sort the pipeline with a Depth First Sort
  // =====================================================

  // Step 1: Determine the nodes with no incoming connections (no inputs)
  std::unordered_map<std::string, bool> has_inputs;
  for (const auto& node : data.items())
  {
    std::string id = node.key();
    has_inputs[id] = false; // Assume no inputs initially

    // Check if there are any incoming connections
    if (node.value().contains("inputs"))
    {
      for (const auto& input : node.value()["inputs"])
      {
        if (!input.is_null() && !input.empty())
        {
          has_inputs[id] = true;
          break;
        }
      }
    }
  }

  /*for (const auto& pair : has_inputs)
   std::cout << "Stage: " << pair.first.substr(0,6) << " has input: " << std::boolalpha << pair.second << std::endl;*/

  // Step 2: Perform topological sorting based on the presence of inputs
  std::vector<std::string> topological_order;
  std::unordered_map<std::string, bool> visited;

  // Initialize visited map
  for (const auto& node : data.items())
  {
    visited[node.key()] = false;
  }

  // Traverse through each node and perform DFS if it hasn't been visited
  for (const auto& node : data.items())
  {
    std::string id = node.key();
    if (!visited[id])
    {
      //std::cout << "Get topological order for: " << id.substr(0,6)  << std::endl;
      get_topological_order(data, id, visited, topological_order);
    }
  }

  // Reverse the order to get nodes without inputs first
  std::reverse(topological_order.begin(), topological_order.end());


  /*
   for (const auto& id : topological_order)
   {
   std::cout << "ID: " << id.substr(0,6) << ", Name: " << data[id]["name"] << std::endl;

   // Optionally, print connections
   if (data[id].contains("inputs")) {
   std::cout << "  Inputs:" << std::endl;
   for (const auto& input : data[id]["inputs"].items()) {
   std::cout << "    " << input.key() << ":" << std::endl;
   for (const auto& conn : input.value()["connections"]) {
   std::cout << "      Node: " << conn["node"] << ", Input: " << conn["input"] << std::endl;
   }
   }
   }
   if (data[id].contains("outputs")) {
   std::cout << "  Outputs:" << std::endl;
   for (const auto& output : data[id]["outputs"].items()) {
   std::cout << "    " << output.key() << ":" << std::endl;
   for (const auto& conn : output.value()["connections"]) {
   std::cout << "      Node: " << conn["node"] << ", Output: " << conn["output"] << std::endl;
   }
   }
   }
   }*/

  // Step 3: Create simplified pipeline JSON
  nlohmann::json simplified_pipeline;
  nlohmann::json pipeline_json = nlohmann::json::array();

  for (const auto& id : topological_order)
  {
    nlohmann::json node_json;
    node_json["uid"] = id;
    node_json["algoname"] = data[id]["name"];

    // Copy the data of the node, if present
    if (data[id].contains("data"))
    {
      for (const auto& [key, value] : data[id]["data"].items())
      {
        node_json[key] = convert_if_numeric(value); // Convert numeric strings to numbers
      }
    }

    // Connect second input for stage that use another stage
    if (data[id].contains("inputs"))
    {
      auto inputs = data[id]["inputs"];
      if (inputs.size() > 1)
      {
        std::cout << inputs.dump(4) << std::endl;
        int i = 0;
        for (const auto& input : inputs.items())
        {
          if (i > 0)
          {
            for (const auto& conn : input.value()["connections"])
            {
              node_json["connect"] = conn["node"];
            }
          }
          i++;
        }
      }
    }

    pipeline_json.push_back(node_json);
  }

  simplified_pipeline["processing"] = processing_json;
  simplified_pipeline["pipeline"] = pipeline_json;

  return simplified_pipeline;
}

nlohmann::json DrawflowParser::convert_if_numeric(const nlohmann::json& value)
{
  if (value.is_string())
  {
    std::string s = value.get<std::string>();
    if (is_number(s))
    {
      std::istringstream iss(s);
      double d;
      iss >> d;
      return d;
    }
  }

  return value; // Return the original value if it's not a numeric string
}

bool DrawflowParser::is_number(const std::string& s)
{
  std::istringstream iss(s);
  double d;
  char c;
  return iss >> d && !(iss >> c); // Checks if the entire string is a valid number
}

// Function to recursively get nodes in topological order
void DrawflowParser::get_topological_order(const nlohmann::json& data, const std::string& current_id, std::unordered_map<std::string, bool>& visited, std::vector<std::string>& topological_order)
{
  // Mark the current node as visited
  visited[current_id] = true;

  // Visit all connected nodes first (DFS)
  if (data.at(current_id).contains("outputs"))
  {
    //std::cout << data.at(current_id).dump(4) << std::endl;
    const auto& outputs = data.at(current_id)["outputs"];
    for (const auto& output : outputs.items())
    {
      std::string output_name = output.key(); // Output key

      //std::cout << "output name :" <<  output_name << std::endl;

      const auto& connections = output.value()["connections"];

      //std::cout << connections.dump(4) << std::endl;

      for (const auto& connection : connections)
      {
        std::string next_id = connection["node"];
        if (!visited[next_id])
        {
          get_topological_order(data, next_id, visited, topological_order);
        }
      }
    }
  }

  // After all connected nodes are visited, add the current node to the topological order
  topological_order.push_back(current_id);
}

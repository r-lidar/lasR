#ifndef DRAWFLOWPARSER_H
#define DRAWFLOWPARSER_H

#include "nlohmann/json.hpp"

class DrawflowParser
{
public:
  static nlohmann::json parse(nlohmann::json json);

private:
  static nlohmann::json convert_if_numeric(const nlohmann::json& value);
  static bool is_number(const std::string& s);
  static void get_topological_order(const nlohmann::json& data, const std::string& current_id, std::unordered_map<std::string, bool>& visited, std::vector<std::string>& topological_order);
};

#endif

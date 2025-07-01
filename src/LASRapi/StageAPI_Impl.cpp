#include "api.h"

#include <sstream>

namespace api
{
Stage::Stage(const std::string& algoname)
{
  set("algoname", algoname);
  set("uid", generate_uid(12));
  set("output", "");
  set("filter", std::vector<std::string>());
}

void Stage::set(const std::string& key, const Value& value)
{
  attributes[key] = value;
}

bool Stage::has(const std::string& key) const
{
  return attributes.find(key) != attributes.end();
}

Stage::Value Stage::get(const std::string& key) const
{
  return attributes.at(key);
}

nlohmann::json Stage::to_json() const
{
  nlohmann::json j;
  for (const auto& [k, v] : attributes)
  {
    const std::string key = k;
    std::visit([&j, &key](auto&& val) { j[key] = val; }, v);
  }
  return j;
}

std::string Stage::get_name() const
{
  return std::get<std::string>(attributes.at("algoname"));
}

std::string Stage::get_uid() const
{
  return std::get<std::string>(attributes.at("uid"));
}

std::string Stage::to_string() const
{
  std::stringstream ss;
  ss << "-----------\n";
  ss << get_name() << " (uid:" << get_uid() << ")\n";

  for (const auto& [key, value] : attributes)
  {
    if (key == "uid" || key == "algoname")
      continue;

    ss << "  " << key << " : ";
    std::visit([&ss](auto&& val) {
      using T = std::decay_t<decltype(val)>;
      if constexpr (std::is_same_v<T, std::string>)
      {
        ss << val;
      }
      else if constexpr (std::is_same_v<T, double>)
      {
        ss << std::fixed << std::setprecision(2) << val;
      }
      else if constexpr (std::is_same_v<T, bool>)
      {
        ss << (val ? "true" : "false");
      }
      else if constexpr (std::is_same_v<T, std::vector<int>> ||
                         std::is_same_v<T, std::vector<bool>> ||
                         std::is_same_v<T, std::vector<double>> ||
                         std::is_same_v<T, std::vector<std::string>>)
      {
        ss << "[";
        for (size_t i = 0; i < val.size(); ++i)
        {
          if (i > 0) ss << ", ";
          ss << val[i];
        }
        ss << "]";
      }
      else
      {
        ss << val;
      }
    }, value);
    ss << " \n";
  }
  return ss.str();
}


std::string Stage::generate_uid(int size)
{
  static const char chars[] = "abcdef0123456789";
  std::string result;
  result.reserve(size);
  for (int i = 0; i < size; ++i)
    result += chars[rand() % (sizeof(chars) - 1)];

  return result;
}

}
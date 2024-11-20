#include "FilterParser.h"
#include "AttributeMapping.h"

#include <numeric>
#include <stdexcept>

std::string FilterParser::parse(const std::string& condition) const
{
  if (condition.empty() || condition[0] == '-')
  {
    return condition; // Already a flag, return as-is
  }

  static const std::vector<std::string> operators = {"%between%", "%in%", "%out%", "==", "!=", ">=", "<=", ">", "<"};

  // Find the operator in the condition
  std::string op;
  size_t opPos = std::string::npos;
  for (const auto& candidate : operators)
  {
    opPos = condition.find(candidate);
    if (opPos != std::string::npos)
    {
      op = candidate;
      break;
    }
  }

  if (op.empty())
  {
    throw std::invalid_argument("Invalid condition: no operator found");
  }

  // Extract left-hand side (LHS), operator, and right-hand side (RHS)
  std::string lhs = trim(condition.substr(0, opPos));
  std::string rhs = trim(condition.substr(opPos + op.length()));

  // Process based on the operator
  if (op == "==")
  {
    return "-lasr_" + lhs + "_equal " + rhs;
  }
  else if (op == "!=")
  {
    return "-lasr_" + lhs + "_different " + rhs;
  }
  else if (op == ">")
  {
    return "-lasr_" + lhs + "_above " + rhs;
  }
  else if (op == "<")
  {
    return "-lasr_" + lhs + "_below " + rhs;
  }
  else if (op == ">=")
  {
    return "-lasr_" + lhs + "_aboveeq " + rhs;
  }
  else if (op == "<=")
  {
    return "-lasr_" + lhs + "_beloweq " + rhs;
  }
  else if (op == "%in%")
  {
    std::vector<std::string> values = split(rhs, ' ');
    return "-lasr_" + lhs + "_in " + std::accumulate(std::next(values.begin()), values.end(), values[0], [](std::string a, std::string b) { return a + " " + b; });
  }
  else if (op == "%out%")
  {
    std::vector<std::string> values = split(rhs, ' ');
    return "-lasr_" + lhs + "_out " + std::accumulate(std::next(values.begin()), values.end(), values[0], [](std::string a, std::string b) { return a + " " + b; });
  }
  else if (op == "%between%")
  {
    // Expect two values for "between"
    std::vector<std::string> values = split(rhs, ' ');
    if (values.size() != 2) throw std::invalid_argument("Invalid condition: %between% must have two values");
    return "-lasr_" + lhs + "_between " + values[0] + " " + values[1] ;
  }

  throw std::invalid_argument("Invalid condition: unsupported operator");
}

std::vector<std::string> FilterParser::split(const std::string& str, char delimiter) const
{
  std::vector<std::string> tokens;
  size_t start = 0;
  size_t end = 0;

  while ((end = str.find(delimiter, start)) != std::string::npos)
  {
    tokens.push_back(trim(str.substr(start, end - start)));
    start = end + 1;
  }

  tokens.push_back(trim(str.substr(start)));

  return tokens;
}

std::string FilterParser::trim(const std::string& str) const
{
  size_t start = str.find_first_not_of(" \t");
  size_t end = str.find_last_not_of(" \t");
  return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

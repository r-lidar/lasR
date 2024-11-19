#include "FilterParser.h"
#include "AttributeMapping.h"

std::string FilterParser::parse(const std::string& condition) const
{
  if (condition.empty() || condition[0] == '-')
  {
    return condition; // Already a flag, return as-is
  }

  for (const auto& [type, pattern] : patterns)
  {
    std::smatch match;

    if (std::regex_match(condition, match, pattern))
    {
      std::string attribute = map_attribute(match[1].str());
      std::string value1 = match[2].str();
      std::string value2 = match.size() > 3 ? match[3].str() : "";

      if (type == "above")
        return "-lasr_" + attribute + "_above " + value1;
      if (type == "aboveeq")
        return "-lasr_" + attribute + "_aboveeq " + value1;
      else if (type == "lower")
        return "-lasr_" + attribute + "_below " + value1;
      if (type == "lowereq")
        return "-lasr_" + attribute + "_beloweq " + value1;
      else if (type == "between")
        return "-lasr_" + attribute + "_between " + value1 + " " + value2;
      else if (type == "equal")
        return "-lasr_" + attribute + "_equal " + value1;
      else if (type == "different")
        return "-lasr_" + attribute + "_different " + value1;
      else if (type == "in" || type == "out")
      {
        size_t start = 0;
        size_t end = value1.find(' ');
        std::string values;

        while (end != std::string::npos)
        {
          if (!values.empty())
          {
            values += " ";  // Add space between values
          }
          values += value1.substr(start, end - start);
          start = end + 1;
          end = value1.find(' ', start);
        }

        // Add the last value after the last space
        if (start < value1.length())
        {
          if (!values.empty()) values += " ";
          values += value1.substr(start);
        }

        // Return the flag with the values
        if (type == "in")
          return "-lasr_" + attribute + "_in " + values;
        else
          return "-lasr_" + attribute + "_out " + values;
      }
    }
  }

  throw std::invalid_argument("Condition not recognized: " + condition);
}

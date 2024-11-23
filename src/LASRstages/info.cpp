#include "info.h"

#include "lasreader.hpp"

#include <cstdint>
#include <inttypes.h>
#include <filesystem>

static std::string human_readable(uint64_t value, const std::vector<std::string>& units)
{
  int unitIndex = 0;
  double displayValue = static_cast<double>(value);

  while (displayValue >= 1000 && unitIndex < (int)units.size() - 1)
  {
    displayValue /= 1000;
    unitIndex++;
  }

  char buffer[32];
  snprintf(buffer, 32, "%.2f %s", displayValue, units[unitIndex].c_str());
  return std::string(buffer);
}


bool LASRinfo::process(Header*& h)
{
  const std::vector<std::string> numberUnits = {"", "thousands", "millions", "billions", "trillions"};
  const std::vector<std::string> byteUnits = {"B", "kB", "MB", "GB", "TB"};

  bool compressed = false;
  uint64_t npoints = h->number_of_point_records;
  uint64_t fsize = npoints*h->schema.total_point_size;

  std::string filePath = ifile;
  bool fileExist = std::filesystem::exists(filePath);

  print("Size    : %s\n", human_readable(fsize, byteUnits).c_str());
  print("Extent       : %.2lf %.2lf %.2lf %.2lf (xmin, xmax, ymin, ymax)\n", h->min_x, h->max_x, h->min_y, h->max_y);
  print("Points       : %s\n", human_readable(npoints, numberUnits).c_str());
  print("Coord. ref.  : %s\n", crs.get_crs().GetName());
  print("Schema:\n"); h->schema.dump();

  return true;
}

bool LASRinfo::set_chunk(Chunk& chunk)
{
  ifile = chunk.main_files[0];
  return true;
}
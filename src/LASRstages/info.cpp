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

LASRinfo::LASRinfo()
{
  printed = false;
}

bool LASRinfo::process(Header*& h)
{
  if (printed) return true;

  const std::vector<std::string> numberUnits = {"", "thousands", "millions", "billions", "trillions"};
  const std::vector<std::string> byteUnits = {"B", "kB", "MB", "GB", "TB"};
  std::string areaUnits; if (crs.is_meters()) areaUnits = "m²"; else if (crs.is_feets()) areaUnits = "ft²"; else areaUnits = "units²";

  bool compressed = false;
  uint64_t npoints = h->number_of_point_records;
  uint64_t fsize = npoints*h->schema.total_point_size;

  std::string filePath = ifile;
  bool fileExist = std::filesystem::exists(filePath);

  print("Source       : %s (v%d.%d)\n", h->signature.c_str(), h->version_major, h->version_minor);
  print("Size         : %s\n", human_readable(fsize, byteUnits).c_str());
  print("Extent       : %.2lf %.2lf %.2lf %.2lf (xmin, xmax, ymin, ymax)\n", h->min_x, h->max_x, h->min_y, h->max_y);
  print("Points       : %s\n", human_readable(npoints, numberUnits).c_str());
  print("Density      : %.1lf pts/%s\n", npoints/((h->max_x - h->min_x) * (h->max_y - h->min_y)), areaUnits.c_str());
  print("Coord. ref.  : %s\n", crs.get_crs().GetName());
  print("Schema       :\n"); h->schema.dump();

  printed = true;

  return true;
}

bool LASRinfo::set_chunk(Chunk& chunk)
{
  ifile = chunk.main_files[0];
  return true;
}
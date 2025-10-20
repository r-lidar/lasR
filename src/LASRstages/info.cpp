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
  std::string linearUnits; if (crs.is_meters()) linearUnits = "m"; else if (crs.is_feets()) linearUnits = "ft"; else linearUnits = "units";

  bool compressed = false;
  uint64_t npoints = h->number_of_point_records;
  uint64_t fsize = npoints*h->schema.total_point_size;
  double area = (h->max_x - h->min_x) * (h->max_y - h->min_y);

  std::string filePath = ifile;
  bool fileExist = std::filesystem::exists(filePath);

  print("Source       : %s (v%d.%d)\n", h->signature.c_str(), h->version_major, h->version_minor);
  print("Size         : %s\n", human_readable(fsize, byteUnits).c_str());
  print("Extent       : %.2lf %.2lf %.2lf %.2lf (xmin, xmax, ymin, ymax)\n", h->min_x, h->max_x, h->min_y, h->max_y);
  print("Points       : %s\n", human_readable(npoints, numberUnits).c_str());
  print("Area         : %.1lf %s\n", area, areaUnits.c_str());
  print("Density      : %.1lf pts/%s\n", npoints/(area), areaUnits.c_str());
  print("Coord. ref.  : %s\n", crs.get_crs().GetName());

  if (h->copc_root_spacing > 0 && h->copc_points_per_level.size() > 0)
  {
    print("COPC structure  : octree with %lu levels\n", h->copc_points_per_level.size());
    uint64_t ntot_points = 0;
    for (size_t i = 0 ; i < h->copc_points_per_level.size() ; i++)
    {
      double spacing = h->copc_root_spacing/std::pow(2, i);
      unsigned int npoints = h->copc_points_per_level[i];
      unsigned int nvoxels = h->copc_voxels_per_level[i];
      ntot_points += npoints;
      double density = (double)ntot_points/area;
      print("    Level %lu: %u points in %u voxels\n", i, npoints, nvoxels);
      print("       - Point spacing: %.2lf %s\n", spacing, linearUnits.c_str());
      print("       - Estimated local density: %.2lf pts/%s\n", (double)npoints/area, areaUnits.c_str());
      print("       - Estimated cumulated density: %.2lf pts/%s\n", density, areaUnits.c_str());
    }
  }

  print("Schema       :\n");
  AttributeSchema& schema = h->schema;
  print("%d attributes | %d bytes per points\n", schema.num_attributes(), schema.total_point_size);
  for (const auto& attr : schema.attributes)
  {
    if (verbose)
      print(" Name: %-17s | Address offset: %-2zu | Size: %-1zu | Type: %-6s | Scale Factor: %-5.3f | Value Offset: %-5.3f\n", attr.name.c_str(), attr.offset, attr.size, attr.attributeTypeToString(), attr.scale_factor, attr.value_offset);
    else
      print(" Name: %-17s | %-6s | Desc: %s\n", attr.name.c_str(), attr.attributeTypeToString(), attr.description.c_str());
  }

  printed = true;

  return true;
}

bool LASRinfo::set_chunk(Chunk& chunk)
{
  ifile = chunk.main_files[0];
  printed = false;
  return true;
}
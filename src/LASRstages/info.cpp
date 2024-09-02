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


bool LASRinfo::process(LASheader*& h)
{
  const std::vector<std::string> numberUnits = {"", "thousands", "millions", "billions", "trillions"};
  const std::vector<std::string> byteUnits = {"B", "kB", "MB", "GB", "TB"};

  bool compressed = h->laszip != nullptr;
  uint64_t npoints = MAX(h->number_of_point_records, h->extended_number_of_point_records);
  uint64_t fsize = std::filesystem::file_size(ifile);

  if (compressed)
    print("LAS v%d.%d format %d\n", h->version_major, h->version_minor, h->point_data_format);
  else
    print("LAS v%d.%d format %d\n", h->version_major, h->version_minor, h->point_data_format);

  print("File size    : %s", human_readable(fsize, byteUnits).c_str());

  if (compressed)
  {
    uint64_t uncompress_size = npoints * h->point_data_record_length;
    double compression_ratio = (double)uncompress_size/(double)fsize;
    print(" (laz) %s (las) compression ratio %.1lf", human_readable(uncompress_size, byteUnits).c_str(), compression_ratio);
  }
  print("\n");

  print("Extent       : %.3lf %.3lf %3lf %.3lf (xmin, xmax, ymin, ymax)\n", h->min_x, h->max_x, h->min_y, h->max_y);
  print("Points       : %s\n", human_readable(npoints, numberUnits).c_str());
  print("Coord. ref.  : %s\n", crs.get_crs().GetName());
  print("Spatial index: ");

  const char* file = ifile.c_str();
  LASreadOpener lasreadopener;
  lasreadopener.set_file_name(file);
  LASreader* lasreader = lasreadopener.open();
  if (lasreader)
  {
    if (lasreader->get_copcindex())
      print("COPC");
    else if (lasreader->get_index())
      print("LAX");
    else
      print("none supported");
  }
  else
  {
    print("failure");
  }
  print("\n");

  print("Extrabytes   :\n");

  int nextrabytes = 0;
  const char* name_table[10] = { "unsigned char", "char", "unsigned short", "short", "unsigned long", "long", "unsigned long long", "long long", "float", "double" };
  for (int i = 0 ; i < h->number_of_variable_length_records ; i++)
  {
    LASvlr vlr = h->vlrs[i];

    if (strcmp(vlr.user_id, "LASF_Spec") != 0 || vlr.record_id != 4)
      continue;

    nextrabytes++;
    int s = strnlen(vlr.description, 32);
    if (s > 0)
      print(" %s\n", vlr.description);
    else
      print(" (no description)\n", vlr.description);

    for (int j = 0; j < vlr.record_length_after_header; j += 192)
    {
      int type = ((I32)(vlr.data[j+2])-1)%10;
      int dim = ((I32)(vlr.data[j+2])-1)/10+1;

      print("   %s : %s (%s) |", (char*)(vlr.data + j + 4), (char*)(vlr.data + j + 160), name_table[type]);
      print(" range [");

      if (vlr.data[j+3] & 0x02) // if min is set
      {
        for (int k = 0; k < dim; k++)
        {
          if (type < 8)
            print("%" PRId64, ((int64_t*)(vlr.data + j + 64))[k]);
          else
            print("%g ", ((double*)(vlr.data + j + 64))[k]);
        }
      }
      else
      {
        print("na ");
      }

      if (vlr.data[j+3] & 0x04) // if max is set
      {
        for (int k = 0; k < dim; k++)
        {
          if (type < 8)
            print(", %" PRId64, ((int64_t*)(vlr.data + j + 88))[k]);
          else
            print(" %g", ((double*)(vlr.data + j + 88))[k]);
        }
      }
      else
      {
        print("na");
      }
      print("]");

      if (vlr.data[j+3] & 0x08) // if scale is set
      {
        print(" scale:");
        for (int k = 0; k < dim; k++)
        {
          print(" %g", ((double*)(vlr.data + j + 112))[k]);
        }
      }
      else
      {
        print(" scale: 1 (not set)");
      }

      if (vlr.data[j+3] & 0x10) // if offset is set
      {
        print(" offset:");
        for (int k = 0; k < dim; k++)
        {
          print(" %g", ((double*)(vlr.data + j + 136))[k]);
        }
      }
      else
      {
        print(" offset: 0 (not set)");
      }

      print("\n");
    }
  }

  return true;
}

bool LASRinfo::set_chunk(Chunk& chunk)
{
  ifile = chunk.main_files[0];
  return true;
}
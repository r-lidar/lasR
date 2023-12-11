#include "writelax.h"
#include "macros.h"

#include "laswriter.hpp"
#include "lasreader.hpp"
#include "laszip_decompress_selective_v3.hpp"
#include "lasindex.hpp"
#include "lasquadtree.hpp"

bool LASRlaxwriter::set_chunk(const Chunk& chunk)
{
  std::vector<std::string> files;
  files.insert(files.end(), chunk.main_files.begin(), chunk.main_files.end());
  files.insert(files.end(), chunk.neighbour_files.begin(), chunk.neighbour_files.end());

  for (const auto& file : files)
  {
    // Initialize las objects
    const char* filechar = const_cast<char*>(file.c_str());
    LASreadOpener lasreadopener;
    lasreadopener.set_file_name(filechar);
    LASreader* lasreader = lasreadopener.open();

    if (!lasreader)
    {
      last_error = "LASlib internal error"; // # nocov
      return false; // # nocov
    }

    // This file is already indexed
    if (lasreader->get_copcindex() || lasreader->get_index())
    {
      lasreader->close();
      delete lasreader;
      return false;
    }

    lasreadopener.set_decompress_selective(LASZIP_DECOMPRESS_SELECTIVE_CHANNEL_RETURNS_XY);

    // setup the quadtree
    LASquadtree* lasquadtree = new LASquadtree;

    float w = lasreader->header.max_x - lasreader->header.min_x;
    float h = lasreader->header.max_y - lasreader->header.min_y;
    F32 t;

    if ((w < 1000) && (h < 1000))
      t = 10.0;
    else if ((w < 10000) && (h < 10000)) // # nocov start
      t = 100.0;
    else if ((w < 100000) && (h < 100000))
      t = 1000.0;
    else if ((w < 1000000) && (h < 1000000))
      t = 10000.0;
    else
      t = 100000.0; // # nocov end

    lasquadtree->setup(lasreader->header.min_x, lasreader->header.max_x, lasreader->header.min_y, lasreader->header.max_y, t);

    LASindex lasindex;
    lasindex.prepare(lasquadtree, 1000);

    progress->reset();
    progress->set_prefix("Write LAX");
    progress->set_total(lasreader->header.number_of_point_records);

    while (lasreader->read_point())
    {
      lasindex.add(lasreader->point.get_x(), lasreader->point.get_y(), (U32)(lasreader->p_count-1));
      (*progress)++;
      progress->show();
    }

    lasreader->close();
    delete lasreader;

    int minimum_points = 100000;
    int maximum_intervals = -20;
    lasindex.complete(minimum_points, maximum_intervals, false);

    if (!lasindex.append(lasreadopener.get_file_name()))
    {
      lasindex.write(lasreadopener.get_file_name());
    }

    progress->done();
  }

  return true;
}
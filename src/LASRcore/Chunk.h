#ifndef CHUNK_H
#define CHUNK_H

#include "Shape.h"
#include <string>

struct Chunk
{
  Chunk()
  {
    clear();
  };

  bool is_empty()
  {
    return main_files.empty();
  }

  void clear()
  {
    xmin = 0;
    ymin = 0;
    xmax = 0;
    ymax = 0;
    id = 0;
    shape = ShapeType::UNKNOWN;
    buffer = 0;
    process = true;
    strict_clip = false;
    catalog_xmax = 0;
    catalog_ymax = 0;
    name.clear();
    main_files.clear();
    neighbour_files.clear();
  };

  // # nocov start
  void dump() const
  {
    printf("name: %s\n", name.c_str());
    printf("bbox %.1lf %.1lf %.1lf %.1lf\n", xmin, ymin, xmax, ymax);
    printf("buffer %.1lf\n", buffer);
    printf("Files:\n");
    for (const auto& file : main_files) printf("  %s\n", file.c_str());
    printf("Neighbour:\n");
    for (const auto& file : neighbour_files) printf("  %s\n", file.c_str());
  };
  // # nocov end

  double xmin;
  double ymin;
  double xmax;
  double ymax;
  double buffer;
  bool process;
  int id;
  ShapeType shape;
  bool strict_clip;
  // Outer boundary at/beyond which strict_clip_decide treats
  // px == xmax as CORE (the "rightmost cell carve-out"). For
  // catalog-wide reads this equals the catalog's xmax/ymax; for
  // AOI-derived sub-queries it's the clamped AOI bbox (so
  // partitioned and un-partitioned AOI reads agree on
  // boundary-coincident points).
  double catalog_xmax;
  double catalog_ymax;
  std::string name;
  std::vector<std::string> main_files;
  std::vector<std::string> neighbour_files;
};

#endif
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
  std::string name;
  std::vector<std::string> main_files;
  std::vector<std::string> neighbour_files;
};

#endif
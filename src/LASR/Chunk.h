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
    shape = ShapeType::UNKNOWN;
    buffer = 0;
    name.clear();
    main_files.clear();
    neighbour_files.clear();
  };

  double xmin;
  double ymin;
  double xmax;
  double ymax;
  double buffer;
  ShapeType shape;
  std::string name;
  std::vector<std::string> main_files;
  std::vector<std::string> neighbour_files;
};

#endif
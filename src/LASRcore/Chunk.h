#ifndef CHUNK_H
#define CHUNK_H

#include "Shape.h"
#include <string>
#include <vector>
#include <algorithm>

// The exact footprint of a chunk as a closed polygon (the last vertex is not repeated). It is
// set only when a stage changes the coordinates in a way that makes the chunk box inexact: after
// transform_crs() the core of a chunk is a curved and rotated shape whose axis-aligned bounding
// box also covers slivers of the neighbouring chunks. It is empty otherwise.
struct Footprint
{
  std::vector<double> x;
  std::vector<double> y;

  bool empty() const { return x.size() < 3; }
  void clear() { x.clear(); y.clear(); }

  // The x coordinates where the horizontal line at 'py' crosses the boundary, sorted. A point
  // (px, py) is inside if an odd number of crossings is greater than px. An edge is crossed if one
  // end is above py and the other is not (half-open rule), and the crossing is computed from the
  // lowest end of the edge, so the neighbouring footprints that share an edge compute the exact
  // same crossing and a point is inside only one of them.
  void crossings(double py, std::vector<double>& xs) const
  {
    xs.clear();
    size_t n = x.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++)
    {
      bool above_i = y[i] > py;
      bool above_j = y[j] > py;
      if (above_i == above_j) continue;
      size_t lo = (y[i] < y[j] || (y[i] == y[j] && x[i] < x[j])) ? i : j;
      size_t hi = (lo == i) ? j : i;
      xs.push_back(x[lo] + (py - y[lo]) * (x[hi] - x[lo]) / (y[hi] - y[lo]));
    }
    std::sort(xs.begin(), xs.end());
  }

  bool contains(double px, double py) const
  {
    if (empty()) return true;
    std::vector<double> xs;
    crossings(py, xs);
    size_t n = xs.end() - std::upper_bound(xs.begin(), xs.end(), px);
    return n % 2 == 1;
  }
};

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
    footprint.clear();
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
  Footprint footprint;
  std::string name;
  std::vector<std::string> main_files;
  std::vector<std::string> neighbour_files;
};

#endif
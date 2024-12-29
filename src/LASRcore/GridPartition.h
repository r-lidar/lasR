#ifndef GRIDPARTITION_H
#define GRIDPARTITION_H

#include <vector>

#include "Grid.h"
#include "Grouper.h"

class LASpoint;

class GridPartition : public Grouper, public Grid
{
public:
  GridPartition(double xmin, double ymin, double xmax, double ymax, double res);
  //GridPartition(double xmin, double ymin, double xmax, double ymax, int nrows, int ncols);
  bool insert(double x, double y);
  void query(double xmin, double ymin, double xmax, double ymax, std::vector<Interval>& res) const;
  static double guess_resolution_from_density(double density);
  //void query(int cell, std::vector<Interval>& res) const;
};

#endif

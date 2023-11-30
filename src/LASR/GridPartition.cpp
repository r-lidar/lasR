#include "GridPartition.h"

GridPartition::GridPartition(double xmin, double ymin, double xmax, double ymax, double res) : Grid(xmin, ymin, xmax, ymax, res)
{
  npoints = 0;
}

/*GridPartition::GridPartition(double xmin, double ymin, double xmax, double ymax, int nrows, int ncols) : Grid(xmin, ymin, xmax, ymax, nrows, ncols)
{
  npoints = 0;
}*/

bool GridPartition::insert(double x, double y)
{
  int key = cell_from_xy(x, y);
  if (key == -1) return false;
  return Grouper::insert(key);
}

void GridPartition::query(double xmin, double ymin, double xmax, double ymax, std::vector<Interval>& res) const
{
  std::vector<int> cells;
  get_cells(xmin, ymin, xmax, ymax, cells);

  for (int cell : cells)
  {
    auto _it = map.find(cell);
    if (_it == map.end()) continue;
    for (auto it = _it->second.begin() ; it != _it->second.end() ; it++)
    {
      res.push_back({it->start, it->end});
    }
  }
}

/*void GridPartition::query(int cell, std::vector<Interval>& res) const
{
  auto _it = map.find(cell);
  if (_it == map.end()) return;
  for (auto it = _it->second.begin() ; it != _it->second.end() ; it++)
  {
    res.push_back({it->start, it->end});
  }
}*/
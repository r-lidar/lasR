#include "filter.h"

bool LASRfilter::process(Point*& p)
{
  if (pointfilter.filter(p))
    p->set_deleted();

  return true;
}

bool LASRfilter::process(PointCloud*& las)
{
  int n = 0;
  Point* p;
  while (las->read_point())
  {
    p = &las->point;
    process(p);
    if (p->get_deleted())
    {
      las->delete_point();
      n++;
    }
  }

  las->update_header();

  // In lasR, deleted points are not actually deleted. They are withheled, skipped by each stage but kept
  // to avoid the cost of memory reallocation and memmove. Here, if we remove more than 33% of the points
  // actually remove the points. This will save some computation later.
  /*double ratio = (double)n/(double)las->npoints;
  if (ratio > 1/3)
  {
    if (!las->delete_withheld()) return false;
  }*/

  return true;
}

bool LASRfiltergrid::set_parameters(const nlohmann::json& stage)
{
  res = stage.at("res");
  std::string op = stage.value("operator", "");

  this->op = MAX;
  if (op == "max") this->op = MAX;
  if (op == "min") this->op = MIN;

  return true;
}

bool LASRfiltergrid::process(PointCloud*& las)
{
  Grid grid(las->header->min_x, las->header->min_y, las->header->max_x, las->header->max_y, res);
  std::vector<PointLAS> selected_points;
  selected_points.resize(grid.get_ncells());
  double v = (op == MIN) ? std::numeric_limits<double>::max() : -std::numeric_limits<double>::max();
  for (auto& seed : selected_points) seed.z = v;

  while (las->read_point())
  {
    if (pointfilter.filter(&las->point)) continue;

    double x = las->point.get_x();
    double y = las->point.get_y();
    double z = las->point.get_z();
    unsigned int id = las->current_point;
    int cell = grid.cell_from_xy(x,y);

    PointLAS& p = selected_points[cell];

    bool pass = false;
    switch (op)
    {
    case MIN: pass = z < p.z; break;
    case MAX: pass = z > p.z; break;
    }

    if (pass)
    {
      p.x = x;
      p.y = y;
      p.z = z;
      p.FID = id;
    }
  }

  std::vector<bool> keep(las->npoints, false);
  for (auto& p : selected_points) if (p.z != v) keep[p.FID] = true;
  int n = std::accumulate(keep.begin(), keep.end(), 0);

  while (las->read_point())
  {
    if (!keep[las->current_point])
      las->delete_point();
  }

  // In lasR, deleted points are not actually deleted. They are withheled, skipped by each stage but kept
  // to avoid the cost of memory reallocation and memmove. Here, if we remove more than 33% of the points
  // actually remove the points. This will save some computation later.
  /*double ratio = (double)n/(double)las->npoints;
  if (ratio > 1/3)
  {
    if (!las->delete_withheld()) return false;
  }*/

  return true;
}
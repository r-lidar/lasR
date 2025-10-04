#include "filter.h"

bool LASRfilter::process(Point*& p)
{
  if (!pointfilter.filter(p))
    p->set_deleted();

  return true;
}

bool LASRfilter::process(PointCloud*& las)
{
  Point* p;
  while (las->read_point())
  {
    p = &las->point;
    process(p);
  }

  las->update_header();
  las->delete_deleted();

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

  las->delete_deleted();

  return true;
}
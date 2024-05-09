#include "filter.h"

bool LASRfilter::process(LASpoint*& p)
{
  if (lasfilter.filter(p))
    p->set_withheld_flag(1);

  return true;
}

bool LASRfilter::process(LAS*& las)
{
  int n = 0;
  LASpoint* p;
  while (las->read_point())
  {
    p = &las->point;
    process(p);
    if (p->get_withheld_flag() != 0)
    {
      las->update_point();
      n++;
    }
  }

  las->update_header();

  // In lasR, deleted points are not actually deleted. They are withheled, skipped by each stage but kept
  // to avoid the cost of memory reallocation and memmove. Here, if we remove more than 33% of the points
  // actually remove the points. This will save some computation later.
  double ratio = (double)n/(double)las->npoints;
  if (ratio > 1/3)
  {
    if (!las->delete_withheld()) return false;
  }

  return true;
}
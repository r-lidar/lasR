#include "filter.h"

bool LASRfilter::process(LASpoint*& p)
{
  if (lasfilter.filter(p))
    p->set_withheld_flag(1);

  return true;
}

bool LASRfilter::process(LAS*& las)
{
  LASpoint* p;
  while (las->read_point())
  {
    p = &las->point;
    process(p);
    if (p->get_withheld_flag() != 0)
      las->update_point();
  }

  las->update_header();

  return true;
}
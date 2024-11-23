#include "nothing.h"

LASRnothing::LASRnothing()
{
}

bool LASRnothing::set_parameters(const nlohmann::json& stage)
{
  bool read = stage.value("read", false);
  bool stream = stage.value("stream", false);
  loop = stage.value("loop", false);

  read_points = read | stream | loop;
  streamable  = read_points & stream & !loop;

  return true;
}

bool LASRnothing::process(LAS*& las)
{
  las->newheader->schema.dump();
  print("Number of points %lu\n", las->npoints);

  AttributeHandler set_and_get_intensity("Intensity");

  if (!loop) return true;

  PointFilter filter;
  filter.add_condition("Intensity > 1000");

  int i = 0;
  while(las->read_point())
  {
    printf("%d | %.2lf %.2lf %.2lf %d\n", i, las->p.get_x(), las->p.get_y(), las->p.get_z(), (int)set_and_get_intensity(&las->p));
    if (filter.filter(&las->p))  set_and_get_intensity(&las->p, 0);
    i++;
    if (i > 10) break;
  }

  las->seek(0);

  i = 0;
  while(las->read_point())
  {
    printf("%d | %.2lf %.2lf %.2lf %d\n", i, las->p.get_x(), las->p.get_y(), las->p.get_z(), (int)set_and_get_intensity(&las->p));
    i++;
    if (i > 10) break;
  }

  return true;
}

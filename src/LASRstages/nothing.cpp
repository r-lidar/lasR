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

bool LASRnothing::process(PointCloud*& las)
{
  if (!loop) return true;

  int i = 0;
  while(las->read_point())
  {
    i++;
  }

  return true;
}

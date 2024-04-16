#include "nothing.h"

LASRnothing::LASRnothing(bool read, bool stream, bool loop)
{

  read_points = read | stream | loop;
  streamable  = read_points & stream & !loop;
  this->loop = loop;
}

bool LASRnothing::process(LAS*& las)
{
  if (!loop) return true;

  int i = 0;
  while(las->read_point())
  {
    i++;
  }

  return true;
}

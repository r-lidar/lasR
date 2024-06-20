#include "breakif.h"

LASRbreakoutsidebbox::LASRbreakoutsidebbox(double xmin, double ymin, double xmax, double ymax) : LASRbreak()
{
  this->xmin = xmin;
  this->ymin = ymin;
  this->xmax = xmax;
  this->ymax = ymax;
}


bool LASRbreakoutsidebbox::set_chunk(const Chunk& chunk)
{
  if (chunk.xmax < xmin || xmax < chunk.xmin) state = true;
  if (chunk.ymax < ymin || ymax < chunk.ymin) state = true;
  return true;
}


bool LASRbreakbeforechunk::set_chunk(const Chunk& chunk)
{
  if (chunk.id < index) state = true;
  return true;
}
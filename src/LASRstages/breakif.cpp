#include "breakif.h"

LASRbreakoutsidebbox::LASRbreakoutsidebbox() : LASRbreak()
{
}

bool LASRbreakoutsidebbox::set_parameters(const nlohmann::json& stage)
{
  xmin = stage.at("xmin");
  ymin = stage.at("ymin");
  xmax = stage.at("xmax");
  ymax = stage.at("ymax");

  return true;
}


bool LASRbreakoutsidebbox::set_chunk(const Chunk& chunk)
{
  if (chunk.xmax < xmin || xmax < chunk.xmin) state = true;
  if (chunk.ymax < ymin || ymax < chunk.ymin) state = true;
  return true;
}

bool LASRbreakbeforechunk::set_parameters(const nlohmann::json& stage)
{
  index = stage.at("index");
  return true;
}


bool LASRbreakbeforechunk::set_chunk(const Chunk& chunk)
{
  if (chunk.id+1 < index) state = true;
  return true;
}
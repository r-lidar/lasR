#ifndef BREAKIF_H
#define BREAKIF_H

#include "Stage.h"

class LASRbreak : public Stage
{
public:
  LASRbreak() { this->state = false; };
  std::string get_name() const override { return "stop_if"; };
  bool break_pipeline() override { return state; };
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  void clear(bool) override { state = false; };

protected:
  bool state;
};

class LASRbreakoutsidebbox : public LASRbreak
{
public:
  LASRbreakoutsidebbox();
  bool set_parameters(const nlohmann::json&) override;
  bool set_chunk(Chunk& chunk) override;
  LASRbreakoutsidebbox* clone() const override { return new LASRbreakoutsidebbox(*this); };
};

class LASRbreakbeforechunk : public LASRbreak
{
public:
  LASRbreakbeforechunk() { };
  bool set_parameters(const nlohmann::json&) override;
  bool set_chunk(Chunk& chunk) override;
  LASRbreakbeforechunk* clone() const override { return new LASRbreakbeforechunk(*this); };

private:
  int index;
};
#endif
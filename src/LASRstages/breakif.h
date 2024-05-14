#ifndef BREAKIF_H
#define BREAKIF_H

#include "Stage.h"

class LASRbreakoutsidebbox : public Stage
{
public:
  LASRbreakoutsidebbox(double xmin, double ymin, double xmax, double ymax);
  std::string get_name() const override { return "stop_if"; };
  bool set_chunk(const Chunk& chunk) override;
  bool break_pipeline() override { return state; };
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  void clear(bool) override { state = false; };

  // multi-threading
  LASRbreakoutsidebbox* clone() const override { return new LASRbreakoutsidebbox(*this); };

private:
  bool state;
};
#endif
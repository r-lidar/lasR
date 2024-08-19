#ifndef LASRFILTER_H
#define LASRFILTER_H

#include "Stage.h"

class LASRfilter : public Stage
{
public:
  bool process(LASpoint*& p) override;
  bool process(LAS*& las) override;
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "filter"; };

  // multi-threading
  LASRfilter* clone() const override { return new LASRfilter(*this); };
};

class LASRfiltergrid : public Stage
{
public:
  bool process(LAS*& las) override;
  double need_buffer() const override { return res; };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "grid filter"; };

  // multi-threading
  LASRfiltergrid* clone() const override { return new LASRfiltergrid(*this); };

private:
  enum operators {MIN, MAX};
  double res;
  int op;
};

#endif
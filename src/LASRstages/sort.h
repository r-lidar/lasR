#ifndef LASRSORT_H
#define LASRSORT_H

#include "Stage.h"

class LASRsort : public Stage
{
public:
  LASRsort() : spatial(false) { };
  bool process(LAS*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "optimize"; };

  // multi-threading
  LASRsort* clone() const override { return new LASRsort(*this); };

private:
  bool spatial;
};

#endif
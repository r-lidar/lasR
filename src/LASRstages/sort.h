#ifndef LASRSORT_H
#define LASRSORT_H

#include "Stage.h"

class LASRsort : public Stage
{
public:
  LASRsort(bool spatial) : spatial(spatial) { };
  bool process(LAS*& las) override;
  std::string get_name() const override { return "optimize"; };

  // multi-threading
  LASRsort* clone() const override { return new LASRsort(*this); };

private:
  bool spatial;
};

#endif
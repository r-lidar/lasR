#ifndef IPF_H
#define IPF_H

#include "Stage.h"

class LASRipf: public Stage
{
public:
  LASRipf() = default;
  bool process(PointCloud*& las) override;
  double need_buffer() const override { return 10; };
  bool is_parallelized() const override { return true; };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "ipf"; };

  // multi-threading
  LASRipf* clone() const override { return new LASRipf(*this); };

private:
  double radius;
  int n;
  int classification;
};

#endif
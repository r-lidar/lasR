#ifndef SOR_H
#define SOR_H

#include "Stage.h"

class LASRsor: public Stage
{
public:
  LASRsor() = default;
  bool process(PointCloud*& las) override;
  double need_buffer() const override { return 10; };
  bool is_parallelized() const override { return true; };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "sor"; };

  // multi-threading
  LASRsor* clone() const override { return new LASRsor(*this); };

private:
  int k;
  int m;
  int classification;
};

#endif
#ifndef IVF_H
#define IVF_H

#include "Stage.h"

class LASRivf: public Stage
{
public:
  LASRivf() = default;
  bool process(PointCloud*& las) override;
  double need_buffer() const override { return std::max(res_x, res_y); };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "ivf"; };

  // multi-threading
  LASRivf* clone() const override { return new LASRivf(*this); };

private:
  double res_x,res_y,res_z;
  int n;
  int classification;
  bool force_map;

};

#endif
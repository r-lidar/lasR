#ifndef LOADMATRIX_H
#define LOADMATRIX_H

#include "Stage.h"

class LASRloadmatrix : public StageMatrix
{
public:
  LASRloadmatrix() = default;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "load_matrix"; };
  bool is_streamable() const override { return true; };
  bool need_points() const override { return false; };

  // multi-threading
  LASRloadmatrix* clone() const override { return new LASRloadmatrix(*this); };
};
#endif
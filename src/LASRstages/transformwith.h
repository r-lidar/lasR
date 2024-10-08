#ifndef LASRTRANSFORMWITH_H
#define LASRTRANSFORMWITH_H

#include "Stage.h"

class LASRtriangulate;

class LASRtransformwith: public Stage
{
public:
  LASRtransformwith() = default;
  bool process(LAS*& las) override;
  bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uuid) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "transform_with"; }
  bool is_parallelized() const override { return true; };

  // multi-threading
  LASRtransformwith* clone() const override { return new LASRtransformwith(*this); };

private:
  int op;
  std::string attribute;
  enum operators {ADD, SUB};
};

#endif
#ifndef FOCAL_H
#define FOCAL_H

#include "Stage.h"

class LASRfocal: public StageRaster
{
public:
  LASRfocal() = default;
  bool process() override;
  double need_buffer() const override { return size; };
  bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uuid) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "focal"; }

  // multi-threading
  LASRfocal* clone() const override { return new LASRfocal(*this); };

private:
  float size;
  float (*operation)(std::vector<float>&);
  static float mean(std::vector<float>& vals);
  static float median(std::vector<float>& vals);
  static float sum(std::vector<float>& vals);
  static float min(std::vector<float>& vals);
  static float max(std::vector<float>& vals);
};

#endif
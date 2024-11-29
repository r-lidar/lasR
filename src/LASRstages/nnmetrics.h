#ifndef NNMETRICS_H
#define NNMETRICS_H

#include "Stage.h"
#include "Metrics.h"

class LASRnnmetrics : public StageVector
{
public:
  LASRnnmetrics() = default;
  bool process(PointCloud*& las) override;
  bool write() override;
  bool set_parameters(const nlohmann::json&) override;
  bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uuid) override;
  std::string get_name() const override { return "neighborhood_metrics"; }
  bool is_parallelized() const override { return true; }
  LASRnnmetrics* clone() const override { return new LASRnnmetrics(*this); };

private:
  int mode;
  int k;
  double r;
  MetricManager metrics;
  std::vector<PointXYZAttrs> lm;
};

#endif

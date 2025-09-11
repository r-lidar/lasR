#ifndef LASRLMF_H
#define LASRLMF_H

#include "Stage.h"
#include "Vector.h"

#include <inttypes.h>
#include <vector>
#include <unordered_map>
#include <memory>

class LASRlocalmaximum : public StageVector
{
public:
  LASRlocalmaximum();
  bool process() override;
  bool process(PointCloud*& las) override;
  bool write() override;
  void clear(bool last) override;
  double need_buffer() const override { return ws; }
  bool need_points() const override { return !use_raster; }
  bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uuid) override;
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "local_maximum"; }
  std::vector<PointLAS>& get_maxima() { return lm; };
  bool is_parallelized() const override { return true; };

  // multi-threading
  LASRlocalmaximum* clone() const override { return new LASRlocalmaximum(*this); };

private:
  bool use_raster;
  bool record_attributes;

  double ws;
  double min_height;

  std::string attribute;
  std::string use_attribute;
  std::vector<PointLAS> lm;

  std::shared_ptr<unsigned int> counter;
  std::shared_ptr<std::unordered_map<uint64_t, unsigned int>> unicity_table;

  enum states {UKN, NLM, LMX};
};

#endif
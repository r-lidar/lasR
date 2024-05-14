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
  LASRlocalmaximum(double xmin, double ymin, double xmax, double ymax, double ws, double min_height, Stage* algorithm);
  LASRlocalmaximum(double xmin, double ymin, double xmax, double ymax, double ws, double min_height, std::string use_sttribute, bool record_attributes = true);
  bool process() override;
  bool process(LAS*& las) override;
  bool write() override;
  void clear(bool last) override;
  double need_buffer() const override { return ws; }
  bool need_points() const override { return !use_raster; }
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

  std::string use_attribute;
  std::vector<PointLAS> lm;

  std::shared_ptr<unsigned int> counter;
  std::shared_ptr<std::unordered_map<uint64_t, unsigned int>> unicity_table;

  enum states {UKN, NLM, LMX};
};

#endif
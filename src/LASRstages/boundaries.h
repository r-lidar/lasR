#ifndef BOUNDARIES_H
#define BOUNDARIES_H

#include "Stage.h"
#include "Vector.h"

class LASRboundaries : public StageVector
{
public:
  LASRboundaries() = default;
  bool process(LASheader*& header) override;
  bool process(LAS*& las) override;
  void clear(bool last) override;
  bool write() override;
  bool need_points() const override;
  bool is_streamable() const override { return true; }
  bool set_parameters(const nlohmann::json&) override;
  bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uuid) override;
  std::string get_name() const override { return "hulls"; }
  bool is_parallelized() const override { return true; };

  // multi-threading
  LASRboundaries* clone() const override { return new LASRboundaries(*this); }

private:
  std::vector<PolygonXY> contour;
};

#endif

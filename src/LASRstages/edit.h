#ifndef EDIT_H
#define EDIT_H

#include "Stage.h"

class LASRedit: public Stage
{
public:
  LASRedit();
  bool process(Point*& p) override;
  bool process(PointCloud*& las) override;
  bool set_parameters(const nlohmann::json&) override;
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "edit"; };

  // multi-threading
  LASRedit* clone() const override { return new LASRedit(*this); };

private:
  std::string attribute;
  double value;
  bool first;
  AttributeAccessor accessor;
};

#endif
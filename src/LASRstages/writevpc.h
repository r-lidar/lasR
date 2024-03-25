#ifndef LASRWRITEVPC_H
#define LASRWRITEVPC_H

#include "Stage.h"

class LASRvpcwriter: public Stage
{
public:
  bool process(LAScatalog*& p) override;
  std::string get_name() const override { return "write_vpc"; }
  bool need_points() const override { return false; }
  bool is_streamable() const override { return true; }

  // Need a clone method to clone a pipeline but anyway this stage is called
  // in pre-run before actually process the pipeline. This stage does not process
  // the point cloud.
  bool is_parallelizable() const override { return true; };
  LASRvpcwriter* clone() const override { return new LASRvpcwriter(*this); };
};

#endif

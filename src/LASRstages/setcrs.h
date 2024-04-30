#ifndef LASRSETCRS_H
#define LASRSETCRS_H

#include "Stage.h"
#include "CRS.h"

class LASRsetcrs: public Stage
{
public:
  LASRsetcrs();
  LASRsetcrs(int epsg);
  LASRsetcrs(const std::string& wkt);
  bool process(LASheader*& p) override;
  void set_crs(const CRS& crs) override { return; };
  std::string get_name() const override { return "set_crs"; }
  bool is_streamable() const override { return true; }

  // Need a clone method to clone a pipeline but anyway this stage is called
  // in pre-run before actually process the pipeline. This stage does not process
  // the point cloud.
  bool is_parallelizable() const override { return true; };
  LASRsetcrs* clone() const override { return new LASRsetcrs(*this); };
};

#endif
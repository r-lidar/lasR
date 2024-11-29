#ifndef LASRWRITELAX_H
#define LASRWRITELAX_H

#include "Stage.h"

class LASRlaxwriter: public Stage
{
public:
  LASRlaxwriter();
  LASRlaxwriter(bool embedded, bool overwrite, bool onthefly);
  bool process(Catalog*& ctg) override;
  bool set_chunk(Chunk& chunk) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  bool set_parameters(const nlohmann::json&) override;
  std::string get_name() const override { return "write_lax"; }
  LASRlaxwriter* clone() const override { return new LASRlaxwriter(*this); };

private:
  bool embedded;
  bool overwrite;
  bool onthefly;
};

#endif

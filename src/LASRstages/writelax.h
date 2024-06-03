#ifndef LASRWRITELAX_H
#define LASRWRITELAX_H

#include "Stage.h"

class LASRlaxwriter: public Stage
{
public:
  LASRlaxwriter(bool embedded, bool overwrite, bool onthefly);
  bool process(LAScatalog*& ctg) override;
  bool set_chunk(const Chunk& chunk) override;
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "write_lax"; }
  LASRlaxwriter* clone() const override { return new LASRlaxwriter(*this); };

private:
  bool write_lax(const std::string& file);

private:
  bool embedded;
  bool overwrite;
  bool onthefly;
};

#endif

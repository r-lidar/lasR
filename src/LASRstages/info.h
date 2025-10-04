#ifndef LASRINFO
#define LASRINFO

#include "Stage.h"
#include "Metrics.h"

#include <map>
#include <vector>
#include <inttypes.h>

class LASRinfo: public Stage
{
public:
  LASRinfo();
  bool set_chunk(Chunk& chunk) override;
  bool process(Header*& header) override;
  bool is_streamable() const override { return true; }
  std::string get_name() const override { return "info"; }

  // multi-threading
  bool is_parallelizable() const override { return false; };
  bool need_points() const override { return false; };
  LASRinfo* clone() const override { return new LASRinfo(*this); };

private:
  bool printed;
};

#endif

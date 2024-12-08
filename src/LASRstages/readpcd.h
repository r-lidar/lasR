#ifndef LASRREADPCD_H
#define LASRREADPCD_H

#include "Stage.h"

#include <fstream>

class LASRpcdreader: public Stage
{
public:
  LASRpcdreader();
  ~LASRpcdreader();
  LASRpcdreader(const LASRpcdreader& other);
  bool process(Header*& header) override;
  bool process(Point*& point) override;
  bool process(PointCloud*& las) override;
  bool set_chunk(Chunk& chunk) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return false; };
  std::string get_name() const override { return "reader_pcd"; }
  void clear(bool) override;

  // multi-threading
  LASRpcdreader* clone() const override { return new LASRpcdreader(*this); };

private:
  Header* header; // ownwed only in streaming mode
  std::ifstream stream;
  std::string line;
  bool is_binary;
};

#endif

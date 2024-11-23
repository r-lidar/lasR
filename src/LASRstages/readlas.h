#ifndef LASRREADLAS_H
#define LASRREADLAS_H

#include "Stage.h"

class LASreadOpener;
class LASreader;
class LASheader;

class LASRlasreader: public Stage
{
public:
  LASRlasreader();
  ~LASRlasreader();
  bool process(Header*& header) override;
  bool process(LASpoint*& point) override;
  bool process(LAS*& las) override;
  bool set_chunk(Chunk& chunk) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "reader_las"; }

  // multi-threading
  LASRlasreader* clone() const override { return new LASRlasreader(*this); };

private:
  Header* header; // not ownwed
  std::vector<AttributeHandler> extrabytes;
  LASreadOpener* lasreadopener;
  LASreader* lasreader;
  LASheader* lasheader;
};

#endif

#ifndef LASRREADEPT_H
#define LASRREADEPT_H

#include "Stage.h"

class EPTio;

class LASReptreader: public Stage
{
public:
  LASReptreader();
  ~LASReptreader();
  bool process(Header*& header) override;
  bool process(Point*& point) override;
  bool process(PointCloud*& las) override;
  bool set_chunk(Chunk& chunk) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "reader_ept"; }
  void clear(bool) override;

  // multi-threading
  LASReptreader* clone() const override { return new LASReptreader(*this); };

private:
  Header* header;
  EPTio* eptio;
  bool streaming;
};

#endif

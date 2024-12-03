#ifndef LASRREADLAS_H
#define LASRREADLAS_H

#include "Stage.h"

class LASlibInterface;

class LASRlasreader: public Stage
{
public:
  LASRlasreader();
  ~LASRlasreader();
  bool process(Header*& header) override;
  bool process(Point*& point) override;
  bool process(PointCloud*& las) override;
  bool set_chunk(Chunk& chunk) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "reader_las"; }
  void clear(bool) override;

  // multi-threading
  LASRlasreader* clone() const override { return new LASRlasreader(*this); };

private:
  Header* header; // ownwed only in streaming mode
  bool streaming;
  LASlibInterface* laslibinterface;
};

#endif

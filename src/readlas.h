#ifndef LASRREADLAS_H
#define LASRREADLAS_H

#include "lasralgorithm.h"

class LASreadOpener;
class LASreader;
class LASheader;

class LASRlasreader: public LASRalgorithm
{
public:
  LASRlasreader();
  ~LASRlasreader();
  bool process(LASheader*& header) override;
  bool process(LASpoint*& point) override;
  bool process(LAS*& las) override;
  bool set_chunk(const Chunk& chunk) override;
  void clear(bool last = false) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  void set_filter(std::string filter) override { this->filter = filter; };
  std::string get_name() const override { return "reader_las"; }

private:
  std::string filter;
  LASreadOpener* lasreadopener;
  LASreader* lasreader;
  LASheader* lasheader;
};

#endif

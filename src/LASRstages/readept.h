#ifndef LASRREADEPT_H
#define LASRREADEPT_H

#include "Stage.h"
#include <memory>
#include "EPTio.h"

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

  void set_ept_index(std::shared_ptr<const EPTio::HierarchyIndex> idx) { ept_index = std::move(idx); }

private:
  Header* header;
  EPTio* eptio;
  bool streaming;
  std::shared_ptr<const EPTio::HierarchyIndex> ept_index;
  bool chunk_strict_clip = false;
  double catalog_xmax = 0;
  double catalog_ymax = 0;
};

#endif

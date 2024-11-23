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
  bool process(Point*& point) override;
  bool process(LAS*& las) override;
  bool set_chunk(Chunk& chunk) override;
  bool need_points() const override { return false; };
  bool is_streamable() const override { return true; };
  std::string get_name() const override { return "reader_las"; }

  // multi-threading
  LASRlasreader* clone() const override { return new LASRlasreader(*this); };

private:
  Header* header; // not ownwed

  AttributeHandler intensity;
  AttributeHandler returnnumber;
  AttributeHandler numberofreturns;
  AttributeHandler userdata;
  AttributeHandler psid;
  AttributeHandler classification;
  AttributeHandler scanangle;
  AttributeHandler gpstime;
  AttributeHandler scannerchannel;
  AttributeHandler red;
  AttributeHandler green;
  AttributeHandler blue;
  AttributeHandler nir;
  std::vector<AttributeHandler> extrabytes;

  LASreadOpener* lasreadopener;
  LASreader* lasreader;
  LASheader* lasheader;
};

#endif

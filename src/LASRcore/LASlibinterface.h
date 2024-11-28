#ifndef LASLIBINTERFACE_H
#define LASLIBINTERFACE_H

#include "LAS.h"
#include "Chunk.h"

#include <string>
#include <vector>
#include <numeric>
#include <stdexcept>

class LASreadOpener;
class LASwriteOpener;
class LASreader;
class LASwriter;
class LASheader;
class LASpoint;
class Progress;

class LASlibInterface
{
public:
  LASlibInterface(Progress*);
  ~LASlibInterface();
  bool open(const Chunk& chunk, std::vector<std::string> filters);
  bool open(const std::string& file);
  bool populate_header(Header* header);
  bool init(const Header* header, const CRS& crs);
  bool read_point(Point* p);
  bool write_point(Point* p);
  bool write_lax(const std::string& file, bool overwrite, bool embedded);
  bool is_opened();
  void close();
  void reset_accessor();
  int64_t p_count();

private:
  LASreadOpener* lasreadopener;
  LASwriteOpener* laswriteopener;
  LASreader* lasreader;
  LASwriter* laswriter;
  LASheader* lasheader;
  LASpoint* point;

  Progress* progress;

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
  std::vector<size_t> extrabytes_offsets;
};


#endif
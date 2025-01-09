#ifndef LASIO_H
#define LASIO_H

#include "PointCloud.h"
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

class LASio
{
public:
  LASio();
  LASio(Progress*);
  ~LASio();
  bool open(const Chunk& chunk, std::vector<std::string> filters);
  bool open(const std::string& file);
  bool create(const std::string& file);
  bool populate_header(Header* header, bool read_first_point = false);
  bool init(const Header* header, const CRS& crs);
  bool read_point(Point* p);
  bool write_point(Point* p);
  bool write_lax(const std::string& file, bool overwrite, bool embedded);
  bool is_opened();
  void close();
  void reset_accessor();
  int64_t p_count();

  // Tools
  static int get_point_data_record_length(int point_data_format, int num_extrabytes = 0);
  static int get_header_size(int minor_version);
  static int guess_point_data_format(bool has_gps, bool has_rgb, bool has_nir);

private:
  LASreadOpener* lasreadopener;
  LASwriteOpener* laswriteopener;
  LASreader* lasreader;
  LASwriter* laswriter;
  LASheader* lasheader;
  LASpoint* point;

  Progress* progress;

  AttributeAccessor intensity;
  AttributeAccessor returnnumber;
  AttributeAccessor numberofreturns;
  AttributeAccessor userdata;
  AttributeAccessor psid;
  AttributeAccessor classification;
  AttributeAccessor scanangle;
  AttributeAccessor gpstime;
  AttributeAccessor scannerchannel;
  AttributeAccessor red;
  AttributeAccessor green;
  AttributeAccessor blue;
  AttributeAccessor nir;
  AttributeAccessor eof_bit;
  AttributeAccessor scandirection_bit;
  AttributeAccessor withheld_bit;
  AttributeAccessor synthetic_bit;
  AttributeAccessor keypoint_bit;
  AttributeAccessor overlap_bit;
  std::vector<AttributeAccessor> extrabytes;
  std::vector<size_t> extrabytes_offsets;
};


#endif

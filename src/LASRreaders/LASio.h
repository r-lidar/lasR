#ifndef LASIO_H
#define LASIO_H

#include "Fileio.h"
#include "Header.h"
#include "PointSchema.h"

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

class LASio : public Fileio
{
public:
  LASio();
  ~LASio();
  void open(const std::string& file) override;
  void create(const std::string& file) override;
  void populate_header(Header* header, bool read_first_point = false) override;
  void init(const Header* header) override;
  bool read_point(Point* p) override;
  bool write_point(Point* p) override;
  void write_lax(const std::string& file, bool overwrite, bool embedded, IProgress* progress = nullptr);
  bool is_opened() override;
  void close() override;
  void reset_accessor() override;
  void set_copc_max_depth(int depth);
  void set_copc_density(int density);
  int64_t p_count() override;

  // Tools
  static int get_point_data_record_length(int point_data_format, int num_extrabytes = 0);
  static int get_header_size(int minor_version);
  static int guess_point_data_format(bool has_gps, bool has_rgb, bool has_nir, bool has_overlap);

  // For lasr library only.
  bool query(const std::vector<std::string>& main_files,
             const std::vector<std::string>& neighbour_files,
             double xmin,
             double ymin,
             double xmax,
             double ymax,
             double buffer,
             bool circle,
             std::vector<std::string> filters);


private:
  LASreadOpener* lasreadopener;
  LASwriteOpener* laswriteopener;
  LASreader* lasreader;
  LASwriter* laswriter;
  LASheader* lasheader;
  LASpoint* point;

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

  int copc_depth;
  int copc_density;
};


#endif

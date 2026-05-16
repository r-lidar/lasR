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
class COPCwriter;

class LASio : public Fileio
{
public:
  LASio();
  ~LASio() noexcept;
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
  void set_copc_max_extra_depth(int extra) { copc_max_extra_depth = extra; }
  void set_copc_max_points_per_chunk(int n) { copc_max_points_per_chunk = n; }
  void set_use_new_copc_writer(bool b) { use_new_copc_writer = b; }
  // Caller-provided bbox that overrides the source header's bbox at COPC
  // open() time. Use when the source header is stale or loose — the writer
  // builds the octree from the declared bbox, so a wrong bbox produces a
  // structurally suboptimal COPC. Has no effect on non-COPC writes.
  void set_bbox_override(double xmin, double ymin, double zmin,
                         double xmax, double ymax, double zmax)
  {
    bbox_override_xmin = xmin; bbox_override_ymin = ymin; bbox_override_zmin = zmin;
    bbox_override_xmax = xmax; bbox_override_ymax = ymax; bbox_override_zmax = zmax;
    bbox_override_set = true;
  }
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
  COPCwriter* copcwriter;
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
  // -1 = no extra cap (writer's hard depth limit applies). 0 = compact:
  // never bump past auto heuristic. >0 = bump up to N levels.
  int copc_max_extra_depth = -1;
  // -1 = use writer default (100k). >0 = user-supplied per-chunk cap.
  int copc_max_points_per_chunk = -1;
  // When true and the output path ends in .copc.laz, route to the new
  // bounded-memory streaming COPCwriter. Otherwise the .copc.laz output is
  // handled by LASlib's in-memory LASwriterCOPC (the historical default).
  bool use_new_copc_writer = false;

  // Source bbox stashed by init() and applied to lasheader by create()
  // when the output is .copc.laz. Lets the COPC writer build its octree
  // with an accurate bbox without touching lasheader on the regular LAZ
  // path (which relies on inventory-at-close for bbox).
  double saved_min_x = 0.0;
  double saved_max_x = 0.0;
  double saved_min_y = 0.0;
  double saved_max_y = 0.0;
  double saved_min_z = 0.0;
  double saved_max_z = 0.0;
  // Source point count stashed by init() and applied to lasheader by
  // create() when the output is .copc.laz. Required so EPToctree's
  // auto max_depth heuristic (which divides total point count by
  // max_points_per_octant=100000) sees the actual count instead of
  // the zero left by init() for the regular LAZ inventory path.
  uint64_t saved_point_count = 0;

  // Optional caller-provided bbox override applied to lasheader before the
  // COPC writer's open() builds the octree. Use when the source header is
  // stale/loose and the caller knows the actual data bbox. Defaults to
  // unset (use the source header's bbox via saved_min_x...).
  double bbox_override_xmin = 0.0;
  double bbox_override_ymin = 0.0;
  double bbox_override_zmin = 0.0;
  double bbox_override_xmax = 0.0;
  double bbox_override_ymax = 0.0;
  double bbox_override_zmax = 0.0;
  bool bbox_override_set = false;
};


#endif

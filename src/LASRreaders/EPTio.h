#ifndef EPTIO_H
#define EPTIO_H

#include "Fileio.h"
#include "PointSchema.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

class LASio;
class Header;
struct Point;

class EPTio : public Fileio
{
public:
  // Octree key identifying a node in the EPT hierarchy.
  // Nested inside EPTio to avoid ODR collision with LASlib's ::EPTkey
  // (defined in lascopc.hpp) in translation units that include both headers.
  struct EPTkey
  {
    int d;
    int x;
    int y;
    int z;
    EPTkey() : d(-1), x(-1), y(-1), z(-1) {}
    EPTkey(int d, int x, int y, int z) : d(d), x(x), y(y), z(z) {}
  };

  EPTio();
  ~EPTio();
  void open(const std::string& endpoint) override;
  void create(const std::string&) override;
  void populate_header(Header* header, bool read_first_point = false) override;
  void init(const Header*) override;
  bool read_point(Point* p) override;
  bool write_point(Point*) override;
  bool is_opened() override;
  void close() override;
  void reset_accessor() override;
  int64_t p_count() override;

  void set_depth(int depth);

  void query(const std::vector<std::string>& main_files,
             const std::vector<std::string>& neighbour_files,
             double xmin, double ymin, double xmax, double ymax,
             double buffer, bool circle,
             std::vector<std::string> filters);

private:
  void parse_ept_json();
  void traverse_hierarchy(double qxmin, double qymin, double qxmax, double qymax);
  void load_hierarchy_page(const EPTkey& key, double qxmin, double qymin, double qxmax, double qymax);
  bool open_next_tile();
  std::string read_file_contents(const std::string& path) const;
  std::string tile_path(const EPTkey& key) const;
  std::string hierarchy_path(const EPTkey& key) const;
  void read_root_tile_header();
  void compute_node_bounds(const EPTkey& key, double& nxmin, double& nymin, double& nzmin,
                           double& nxmax, double& nymax, double& nzmax) const;

private:
  // EPT metadata
  std::string base_path;
  std::string query_string;  // URL query params (e.g. ?token=...) for signed URLs
  bool remote;
  bool opened;
  nlohmann::json ept_metadata;
  double cube_bounds[6];  // octree cube bounds [xmin,ymin,zmin,xmax,ymax,zmax]
  double conf_bounds[6];  // conforming data bounds
  std::string srs_wkt;
  int srs_epsg;
  int span;

  // Depth control
  int depth_limit;

  // Scale/offset from root tile
  double x_scale, y_scale, z_scale;
  double x_offset, y_offset, z_offset;
  bool scale_offset_initialized;

  // Schema info derived from ept.json
  bool has_gps;
  bool has_rgb;
  bool has_nir;

  // Hierarchy traversal state
  std::deque<EPTkey> tile_queue;
  int64_t total_points;

  // Current tile reader
  LASio* current_tile;
  int64_t points_read;
};

#endif

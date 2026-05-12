#ifndef EPTIO_H
#define EPTIO_H

#include "Fileio.h"
#include "PointSchema.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>
#include <deque>
#include <cstdint>
#include <future>
#include <memory>

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

  struct TileEntry
  {
    EPTkey key;
    int64_t point_count;
    double xmin, ymin, zmin, xmax, ymax, zmax;
  };

  struct HierarchyIndex
  {
    nlohmann::json ept_metadata;
    double cube_bounds[6];
    double conf_bounds[6];
    std::string srs_wkt;
    int srs_epsg = 0;
    std::string base_path;
    std::string query_string;
    bool remote = false;
    std::string probe_tile_path;

    std::vector<TileEntry> tiles;
    int64_t total_points = 0;
    bool tiles_built = false;

    static std::shared_ptr<HierarchyIndex> build_metadata(const std::string& endpoint);
    void ensure_tiles();
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
  void set_index(std::shared_ptr<const HierarchyIndex> idx);

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
  // open_tile_sync constructs a LASio at the given key, opens it, and
  // populates the header (so the caller does not block on it again).
  // Returns nullptr on failure (with a warning logged).
  LASio* open_tile_sync(const EPTkey& key);
  void prefetch_next_tile();
  void cancel_prefetch();
  std::string read_file_contents(const std::string& path) const;
  std::string tile_path(const EPTkey& key) const;
  std::string hierarchy_path(const EPTkey& key) const;
  void find_probe_tile();
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

  // Path of a representative tile used by populate_header() to derive the LAS
  // schema via LASio. Using a real LAZ file avoids having to interpret every
  // possible EPT JSON schema naming variation.
  std::string probe_tile_path;

  // Depth control
  int depth_limit;

  // Hierarchy traversal state
  std::deque<EPTkey> tile_queue;
  int64_t total_points;

  // Current tile reader
  LASio* current_tile;
  // Background-prefetched next tile. While read_point drains current_tile,
  // open_next_tile starts an std::async opening the next queued tile in
  // parallel. When current_tile is exhausted we just .get() the future
  // and swap. Single-producer single-consumer; one future per EPTio.
  std::future<LASio*> next_tile_future;
  int64_t points_read;

  // Optional shared metadata index (Task 2+). When set via set_index, future
  // tasks will use this to share metadata across per-chunk readers; currently
  // unused by the existing read path.
  std::shared_ptr<const HierarchyIndex> index;
};

#endif

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
#include <array>

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

    // Walk the EPT hierarchy from the root, populating `tiles` and
    // `total_points`. The default (no AOI filter) caches the full walk via
    // `tiles_built` so subsequent calls are no-ops.
    //
    // When `aoi_bboxes` is non-empty, prune the BFS to subtrees whose node
    // bbox intersects any rect in {xmin,ymin,xmax,ymax}. This is the fast
    // path for partition_ept on huge EPTs (e.g. continent-scale archives)
    // where enumerating the full hierarchy would dominate startup. The
    // result is NOT cached: tiles_built stays false so a later un-filtered
    // call still walks everything. Callers must reuse only inside the same
    // logical AOI scope.
    void ensure_tiles(const std::vector<std::array<double, 4>>& aoi_bboxes = {});
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
  // TileLoadResult carries any warning text the worker would otherwise
  // emit directly. R's C API (and lasR's warning() router) is not safe to
  // call from a non-OpenMP background thread, so the std::async worker
  // accumulates the message and the consumer thread emits it after
  // future.get().
  //
  // For remote tiles, the worker bulk-downloads the .laz bytes and stashes
  // them in GDAL's /vsimem/ filesystem so the consumer reads from RAM
  // (zero network on read_point). vsimem_path is the unique /vsimem/ path
  // owned by this result — caller must VSIUnlink it after closing `tile`,
  // because /vsimem/ files are not freed when LASio::close releases the
  // VSI file handle.
  struct TileLoadResult
  {
    LASio* tile = nullptr;
    std::string warning_msg;
    std::string vsimem_path;
  };

  void parse_ept_json();
  void traverse_hierarchy(double qxmin, double qymin, double qxmax, double qymax);
  void load_hierarchy_page(const EPTkey& key, double qxmin, double qymin, double qxmax, double qymax);
  bool open_next_tile();
  // open_tile_sync constructs a LASio at the given key, opens it, and
  // populates the header (so the caller does not block on it again).
  // Safe to call from a background thread: warning text is returned in
  // TileLoadResult::warning_msg rather than emitted directly.
  TileLoadResult open_tile_sync(const EPTkey& key);
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

  // Current tile reader. For remote tiles, current_vsimem_path is the
  // /vsimem/ key holding the buffered tile bytes — it must be VSIUnlink'd
  // when current_tile is released (otherwise the bytes leak in GDAL's
  // in-memory FS).
  LASio* current_tile;
  std::string current_vsimem_path;
  // Background-prefetched tiles. While read_point drains current_tile,
  // open_next_tile keeps the queue full at prefetch_depth so multiple
  // HTTP fetches stay in flight against the EPT endpoint. AWS S3 is
  // HTTP/1.1-only (no multiplexing), so the only way to overlap is more
  // concurrent connections; depth=4 matches the saturation point we
  // measured on usgs-lidar-public from us-east-2. Configurable per-process
  // via LASR_EPT_PREFETCH env var (default 4, clamped to [1, 32]).
  std::deque<std::future<TileLoadResult>> prefetch_queue;
  int prefetch_depth;
  int64_t points_read;

  // Optional shared metadata index (Task 2+). When set via set_index, future
  // tasks will use this to share metadata across per-chunk readers; currently
  // unused by the existing read path.
  std::shared_ptr<const HierarchyIndex> index;
};

#endif

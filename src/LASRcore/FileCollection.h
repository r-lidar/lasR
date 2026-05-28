#ifndef LASCATALOG_H
#define LASCATALOG_H

#include "error.h"
#include "CRS.h"

#include "Chunk.h"
#include "Shape.h"
#include "Header.h"
#include "EPTio.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>

enum PathType {DIRECTORY, VPCFILE, LASFILE, LAXFILE, PCDFILE, OTHERFILE, MISSINGFILE, UNKNOWNFILE, DATAFRAME, XPTR, REMOTELASFILE, EPTFILE, REMOTEEPTFILE};

class Header;

class FileCollectionIndex
{
private:
  std::vector<Rectangle> bboxes;

public:
  void add(double xmin, double ymin, double xmax, double ymax);
  bool has_overlap(double xmin, double ymin, double xmax, double ymax) const;
  std::vector<int> get_overlaps(double xmin, double ymin, double xmax, double ymax) const;
};


class FileCollection
{
public:
  FileCollection();
  ~FileCollection();
  bool read(const std::vector<std::string>& files, bool progress = false);
  bool write_vpc(const std::string& file, const CRS& crs, bool absolute_path, bool use_gpstime);
  bool is_source_vpc() { return use_vpc; };
  void set_buffer(double buffer) { this->buffer = buffer; };
  void add_query(double xmin, double ymin, double xmax, double ymax);
  void add_query(double xcenter, double ycenter, double radius);
  bool set_noprocess(const std::vector<bool>& b);
  bool set_chunk_size(double size);
  bool set_chunk_size(double size, bool strict_clip);
  bool partition_ept(int target_partitions);
  bool get_chunk(int index, Chunk& chunk) const;
  int get_number_chunks() const;
  int get_number_files() const;
  int get_number_indexed_files() const;
  PathType get_format() const;
  double get_buffer() const { return buffer; };
  double get_xmin() const { return xmin; };
  double get_ymin() const { return ymin; };
  double get_xmax() const { return xmax; };
  double get_ymax() const { return ymax; };
  void set_crs(const CRS& crs) { this->crs = crs; };
  CRS get_crs() const { return crs; };
  const std::vector<std::filesystem::path>& get_files() const;
  bool check_spatial_index();
  void set_all_indexed();
  void clear();
  bool file_exists(std::string& file);
  std::shared_ptr<const EPTio::HierarchyIndex> get_ept_index() const { return ept_index; }
  ShapeType get_query_type(int i) const { return queries[i].shape->type(); }

  // Pick the octree depth at which the union of AOI bboxes (each
  // {xmin, ymin, xmax, ymax}) covers at least `target_partitions`
  // integer cells, capped at min(16, max_tile_depth). Used by
  // partition_ept's with-queries path and exposed for unit tests so
  // the same predicate can be exercised without an EPT endpoint.
  // Returns the chosen depth; 0 ≤ d ≤ min(16, max_tile_depth).
  static int ept_pick_depth_for_aois(
      double cube_xmin, double cube_ymin, double cube_size,
      int max_tile_depth, int target_partitions,
      const std::vector<std::array<double, 4>>& aoi_bboxes);

  #ifdef USING_R
  void add_dataframe(double xmin, double ymin, double xmax, double ymax, int npoints);   // Special to build a FileCollection from a data.frame in R
  void add_xptr(Header header);
  #endif

private:
  bool read_vpc(const std::string& file);
  bool add_las_file(std::string file, bool noprocess = false);
  bool add_pcd_file(std::string file, bool noprocess = false);
  bool add_ept_endpoint(std::string path, bool noprocess = false);
  bool add_header(const Header& header, bool noprocess = false);
  bool get_chunk_regular(int index, Chunk& chunk) const;
  bool get_chunk_with_query(int index, Chunk& chunk) const;
  PathType parse_path(const std::string& path);
  void add_query(double xmin, double ymin, double xmax, double ymax, bool strict_clip);
  void add_query(double xmin, double ymin, double xmax, double ymax, bool strict_clip,
                 double owner_xmax, double owner_ymax);  // NaN values fall through to catalog bounds
  void add_query(double xcenter, double ycenter, double radius, bool strict_clip);

private:

  // Bounding box of the file collection
  double xmin;
  double ymin;
  double xmax;
  double ymax;

  // CRS retained of the overall collection
  CRS crs;

  // reader mode to support R data.frame
  bool use_dataframe;
  bool use_vpc;

  // overall parameter to process
  double buffer;
  double chunk_size;

  // information about each file
  std::vector<Header> headers;
  std::vector<std::filesystem::path> files; // path to files
  std::vector<bool> noprocess;              // the file is not processed and is used only for buffering

  // queries, partial read
  FileCollectionIndex file_index;
  // QueryRecord groups the per-query state that the four previous
  // parallel vectors held independently. Using a struct preserves the
  // size invariant by construction (no way to push the shape without
  // the flags) and makes the rebuild in partition_ept a simple
  // vector swap. The shape is owned via unique_ptr.
  //
  // owner_xmax / owner_ymax are NaN by default → get_chunk_with_query
  // falls back to the catalog's xmax/ymax. AOI sub-queries emitted by
  // partition_ept set these to the clamped AOI bbox so the strict-clip
  // rightmost-edge carve-out fires on the AOI boundary.
  struct QueryRecord {
    std::unique_ptr<Shape> shape;
    bool strict_clip = false;
    double owner_xmax = std::nan("");
    double owner_ymax = std::nan("");
  };
  std::vector<QueryRecord> queries;

  // EPT shared metadata index (built when add_ept_endpoint succeeds)
  std::shared_ptr<EPTio::HierarchyIndex> ept_index;
};

#endif
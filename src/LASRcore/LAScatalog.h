#ifndef LASCATALOG_H
#define LASCATALOG_H

#include "error.h"
#include "CRS.h"

#include "Chunk.h"
#include "Shape.h"

#include <set>
#include <string>
#include <vector>
#include <filesystem>

class LASheader;
class LASreadOpener;
class LASkdtreeRectangles;

enum PathType {DIRECTORY, VPCFILE, LASFILE, LAXFILE, OTHERFILE, MISSINGFILE, UNKNOWNFILE};

class LAScatalog
{
public:
  LAScatalog();
  ~LAScatalog();
  bool read(const std::vector<std::string>& files, bool progress = false);
  bool write_vpc(const std::string& file, const CRS& crs, bool absolute_path);
  void set_buffer(double buffer) { this->buffer = buffer; };
  void add_query(double xmin, double ymin, double xmax, double ymax);
  void add_query(double xcenter, double ycenter, double radius);
  bool set_noprocess(const std::vector<bool>& b);
  bool set_chunk_size(double size);
  bool get_chunk(int index, Chunk& chunk) const;
  int get_number_chunks() const;
  int get_number_files() const;
  int get_number_indexed_files() const;
  double get_buffer() const { return buffer; };
  double get_xmin() const { return xmin; };
  double get_ymin() const { return ymin; };
  double get_xmax() const { return xmax; };
  double get_ymax() const { return ymax; };
  void set_crs(const CRS& crs) { this->crs = crs; };
  CRS get_crs() const { return crs; };
  bool check_spatial_index();
  void build_index();
  void clear();

  #ifdef USING_R
  // Special to build a LAScatalog from a data.frame in R
  void add_bbox(double xmin, double ymin, double xmax, double ymax, int npoints)
  {
    this->npoints.push_back(npoints);
    add_bbox(xmin, ymin, xmax, ymax, true, false);
  };
  #endif

private:
  bool read_vpc(const std::string& file);
  bool add_file(const std::string& file, bool noprocess = false);
  void add_bbox(double xmin, double ymin, double xmax, double ymax, bool indexed, bool noprocess = false);
  void add_wkt(const std::string& wkt);
  void add_epsg(int epsg);
  void add_crs(const LASheader* header);
  bool get_chunk_regular(int index, Chunk& chunk) const;
  bool get_chunk_with_query(int index, Chunk& chunk) const;
  PathType parse_path(const std::string& path);

private:

  // Bounding box of the file collection
  double xmin;
  double ymin;
  double xmax;
  double ymax;

  // A set of CRS because each file may have different CRS so we must check the consistency
  std::set<std::string> wkt_set;
  std::set<int> epsg_set;

  // CRS retained of the overall collection
  CRS crs;

  // reader mode to support R data.frame
  bool use_dataframe;

  // overall parameter to process
  double buffer;
  double chunk_size;

  // information about each file
  std::vector<uint64_t> npoints;            // number of points
  std::vector<bool> indexed;                // the file has a spatial index
  std::vector<bool> noprocess;              // the file is not processed and is used only for buffering
  std::vector<Rectangle> bboxes;            // bounding boxes of the files
  std::vector<std::filesystem::path> files; // path to files
  std::vector<std::pair<unsigned short, unsigned short>> dates;
  std::vector<std::pair<double, double>> zlim;

  // queries, partial read
  LASkdtreeRectangles* laskdtree;
  std::vector<Shape*> queries;
};

#endif
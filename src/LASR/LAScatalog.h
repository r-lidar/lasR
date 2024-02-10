#ifndef LASCATALOG_H
#define LASCATALOG_H

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
  bool write_vpc(const std::string& file);
  void set_buffer(double buffer) { this->buffer = buffer; };
  void add_query(double xmin, double ymin, double xmax, double ymax);
  void add_query(double xcenter, double ycenter, double radius);
  bool set_noprocess(const std::vector<bool>& b);
  void set_chunk_size(double size);
  void set_chunk_is_file();
  void set_wkt(const std::string& wkt) { this->wkt = wkt; };
  std::string get_wkt() const { return wkt; };
  void set_epsg(int epsg) { this->epsg = epsg; };
  int get_epsg() const { return epsg; };
  bool get_chunk(int index, Chunk& chunk);
  int get_number_chunks() const;
  int get_number_files() const;
  int get_number_indexed_files() const;
  double get_buffer() const { return buffer; };
  double get_xmin() const { return xmin; };
  double get_ymin() const { return ymin; };
  double get_xmax() const { return xmax; };
  double get_ymax() const { return ymax; };
  bool check_spatial_index();
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
  bool get_chunk_regular(int index, Chunk& chunk);
  bool get_chunk_with_query(int index, Chunk& chunk);
  PathType parse_path(const std::string& path);

public:
  std::string last_error;

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
  int epsg;
  std::string wkt;

  // reader mode to support R data.frame
  bool use_dataframe;

  // overall parameter to process
  double buffer;
  double chunk_size;

  // information about each file
  std::vector<uint64_t> npoints;            // number of points
  std::vector<bool> indexed;                // the file has a spatial index
  std::vector<bool> noprocess;            // the file is not processed and is used only for buffering
  std::vector<Rectangle> bboxes;            // bounding boxes of the files
  std::vector<std::filesystem::path> files; // path to files

  // queries, partial read
  LASkdtreeRectangles* laskdtree;
  std::vector<Shape*> queries;
};

#endif
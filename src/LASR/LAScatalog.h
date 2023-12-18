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
  bool read(const std::vector<std::string>& files);
  void set_chunk_size(double size) { if (size > 0) chunk_size = size; else chunk_size = 0; };
  void set_chunk_is_file() { chunk_size = 0; };
  void set_buffer(double buffer) { this->buffer = buffer; };
  void add_query(double xmin, double ymin, double xmax, double ymax);
  void add_query(double xcenter, double ycenter, double radius);
  bool get_chunk(int index, Chunk& chunk);
  int get_number_chunks() const;
  int get_number_files() const { return indexed.size(); }
  int get_number_indexed_files() const;
  double get_buffer() const { return buffer; };
  bool check_spatial_index();
  void clear();

  #ifdef USING_R
  // Special to build a LAScatalog from a data.frame in R
  void add_bbox(double xmin, double ymin, double xmax, double ymax)
  {
    add_bbox(xmin, ymin, xmax, ymax, true, false);
  };
  #endif

private:
  bool read_vpc(const std::string& file);
  bool write_vpc(const std::string& file);
  bool add_file(const std::string& file, bool buffer_only = false);
  void add_bbox(double xmin, double ymin, double xmax, double ymax, bool indexed, bool buffer_only = false);
  void add_wkt(const std::string& wkt);
  void add_epsg(int epsg);
  void add_crs(const LASheader* header);
  PathType parse_path(const std::string& path);

public:
  double xmin;
  double ymin;
  double xmax;
  double ymax;
  uint64_t npoints;
  std::string last_error;

  // A set of CRS because each file may have different CRS so we must check the consistency
  std::set<std::string> wkt_set;
  std::set<int> epsg_set;
  int epsg;
  std::string wkt;

private:
  // reader mode
  bool use_dataframe;

  // overall parameter
  double buffer;
  double chunk_size;

  // stores information about each file
  std::vector<bool> indexed;                // the file has a spatial index
  std::vector<bool> buffer_only;            // the file is not processed and is used only for bufferring
  std::vector<Rectangle> bboxes;            // bounding boxes of the files
  std::vector<std::filesystem::path> files; // path to files

  //LASreadOpener* lasreadopener;
  LASkdtreeRectangles* laskdtree;

  // Queries, partial read
  std::vector<Shape*> queries;
};

#endif
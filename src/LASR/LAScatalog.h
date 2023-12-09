#ifndef LASCATALOG_H
#define LASCATALOG_H

#include "Chunk.h"
#include "Shape.h"

#include <string>
#include <vector>
#include <filesystem>

class LASheader;
class LASreadOpener;
class LASkdtreeRectangles;

class LAScatalog
{
public:
  LAScatalog();
  ~LAScatalog();
  void set_chunk_size(double size) { if (size> 0) chunk_size = size; else chunk_size = 1000; };
  void set_chunk_is_file() { chunk_size = 0; };
  void set_buffer(double buffer) { this->buffer = buffer; };
  void add_bbox(double xmin, double ymin, double xmax, double ymax, bool indexed, bool buffer_only = false);
  bool add_file(std::string file, bool buffer_only = false);
  void add_query(double xmin, double ymin, double xmax, double ymax);
  void add_query(double xcenter, double ycenter, double radius);
  bool get_chunk(int index, Chunk& chunk);
  int get_number_chunks() const;
  double get_buffer() const { return buffer; };
  int get_number_files() const { return indexed.size(); }
  int get_number_indexed_files() const;
  bool check_spatial_index();

private:
  void set_crs(const LASheader* header);

public:
  double xmin;
  double ymin;
  double xmax;
  double ymax;
  uint64_t npoints;
  std::string last_error;

  // CRS
  int nocrs_number;
  int crs_conflict_number;
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
#ifndef LASCATALOG_H
#define LASCATALOG_H

#include "Chunk.h"
#include "Shape.h"

#include <string>
#include <vector>

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
  void add_bbox(double xmin, double ymin, double xmax, double ymax, bool indexed);
  bool add_file(std::string file);
  void add_query(double xmin, double ymin, double xmax, double ymax);
  void add_query(double xcenter, double ycenter, double radius);
  bool get_chunk(int index, Chunk& chunk);
  int get_number_chunks() const;
  double get_buffer() const { return buffer; };
  int get_number_files() const { return indexed.size(); }
  int get_number_indexed_files() const;
  void check_spatial_index();

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
  std::vector<bool> indexed;
  std::vector<Rectangle> bboxes;

  LASreadOpener* lasreadopener;
  LASkdtreeRectangles* laskdtree;

  // Queries, partial read
  std::vector<Shape*> queries;
};

#endif
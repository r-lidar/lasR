#ifndef LASCATALOG_H
#define LASCATALOG_H

#include "error.h"
#include "CRS.h"

#include "Chunk.h"
#include "Shape.h"
#include "Header.h"

#include <string>
#include <vector>
#include <filesystem>

enum PathType {DIRECTORY, VPCFILE, LASFILE, LAXFILE, PCDFILE, OTHERFILE, MISSINGFILE, UNKNOWNFILE, DATAFRAME, XPTR};

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

  #ifdef USING_R
  void add_dataframe(double xmin, double ymin, double xmax, double ymax, int npoints);   // Special to build a FileCollection from a data.frame in R
  void add_xptr(Header header);
  #endif

private:
  bool read_vpc(const std::string& file);
  bool add_las_file(std::string file, bool noprocess = false);
  bool add_pcd_file(std::string file, bool noprocess = false);
  bool add_header(const Header& header, bool noprocess = false);
  bool get_chunk_regular(int index, Chunk& chunk) const;
  bool get_chunk_with_query(int index, Chunk& chunk) const;
  PathType parse_path(const std::string& path);

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
  std::vector<Shape*> queries;
};

#endif
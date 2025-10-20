#ifndef PCDIO_H
#define PCDIO_H

#include "Fileio.h"
#include "PointSchema.h" // For AttributeType

#include <string>
#include <vector>
#include <numeric>
#include <fstream>

class Progress;
class Header;
struct Point;

class PCDio : public Fileio
{
public:
  PCDio();
  ~PCDio();
  void open(const std::string& file) override;
  void create(const std::string& file) override;
  void populate_header(Header* header, bool read_first_point = false) override;
  void init(const Header* header) override;
  bool read_point(Point* p) override;
  bool write_point(Point* p) override;
  bool is_opened() override;
  void close() override;
  void set_binary_mode(bool);
  void reset_accessor() override;
  int64_t p_count() override;

  // For lasr library only.
  void query(const std::vector<std::string>& main_files,
             const std::vector<std::string>& neighbour_files,
             double xmin, double ymin, double xmax, double ymax,
             double buffer, bool circle,
             std::vector<std::string> filters);

public:
  bool preread_bbox;

private:
  bool read_ascii_point(Point* p);
  bool read_binary_point(Point* p);
  bool write_ascii_point(Point* p);
  bool write_binary_point(Point* p);
  bool write_bbox(const Header* header, const std::string& bbox_filename);
  bool read_bbox(const std::string& bbox_filename, Header* header);
  char attribute_type_code(AttributeType type);

private:
  const Header* header;
  std::ifstream istream;
  std::ofstream ostream;
  std::string line;
  std::string file;
  int64_t npoints;
  bool is_binary;
  bool (PCDio::*read)(Point*);
  bool (PCDio::*write)(Point*);
};

#endif

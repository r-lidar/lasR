#ifndef FILEIO_H
#define FILEIO_H

#include "Chunk.h"
#include "error.h"

#include <string>
#include <vector>


class Progress;
class Header;
struct Point;

class Fileio
{
public:
  Fileio() : progress(nullptr) {}
  Fileio(Progress* p) : progress(p) {}
  virtual ~Fileio() = default;
  virtual bool open(const Chunk& chunk, std::vector<std::string> filters) = 0;
  virtual bool open(const std::string& file) = 0;
  virtual bool create(const std::string& file) = 0;
  virtual bool populate_header(Header* header, bool read_first_point = false) = 0;
  virtual bool init(const Header* header) = 0;
  virtual bool read_point(Point* p) = 0;
  virtual bool write_point(Point* p) = 0;
  virtual bool is_opened() = 0;
  virtual void close() = 0;
  virtual void reset_accessor() = 0;
  virtual int64_t p_count() = 0;

protected:
  Progress* progress;
};


#endif

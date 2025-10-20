#ifndef FILEIO_H
#define FILEIO_H

#include <string>
#include <vector>

class Header;
struct Point;

class Fileio
{
public:
  Fileio() = default;
  virtual ~Fileio() = default;
  virtual void open(const std::string& file) = 0;
  virtual void create(const std::string& file) = 0;
  virtual void populate_header(Header* header, bool read_first_point = false) = 0;
  virtual void init(const Header* header) = 0;
  virtual bool read_point(Point* p) = 0;
  virtual bool write_point(Point* p) = 0;
  virtual bool is_opened() = 0;
  virtual void close() = 0;
  virtual void reset_accessor() = 0;
  virtual int64_t p_count() = 0;
};

struct IProgress
{
  virtual void reset() = 0;
  virtual void set_total(uint64_t) = 0;
  virtual void show() = 0;
  virtual void done() = 0;
  virtual IProgress& operator++(int) = 0;
  virtual ~IProgress() = default;
};

#endif

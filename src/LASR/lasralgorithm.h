#ifndef LASRALGORITHM_H
#define LASRALGORITHM_H

// R
#ifdef USING_R
#define R_NO_REMAP 1
#include <R.h>
#include <Rinternals.h>
#endif

// STL
#include <string>

// LASlib
#include "laszip.hpp"
#include "laspoint.hpp"
#include "lasfilter.hpp"
#include "lastransform.hpp"

// lasR
#include "LAS.h"
#include "LAScatalog.h"
#include "Raster.h"
#include "Vector.h"
#include "Progress.h"
#include "Rcompatibility.h"
#include "error.h"

class LASRalgorithm
{
public:
  LASRalgorithm();

  virtual ~LASRalgorithm() = 0;
  virtual bool process(LASheader*& header) { return true; };
  virtual bool process(LASpoint*& p) { return true; };
  virtual bool process(LAS*& las) { return true; };
  virtual bool process(LAScatalog*& las) { return true; };
  virtual bool write() { return true; };
  virtual void clear(bool last = false) { return; };
  virtual bool set_chunk(const Chunk& chunk);
  virtual bool set_crs(int epsg) { return false; };
  virtual bool set_crs(std::string wkt) { return false; };
  virtual void set_output_file(std::string file) { ofile = file; };
  virtual void set_input_file_name(std::string file) { return; };
  virtual void set_header(LASheader*& header) { return; };
  virtual bool is_streamable() const { return false; };
  virtual double need_buffer() const { return 0; };
  virtual bool need_points() const { return true; };
  virtual void set_filter(std::string f) { char* s = const_cast<char*>(f.c_str()); lasfilter.parse(s); };
  virtual std::string get_name() const = 0;

  // For multi-threading when processing several files in parallel.
  // Each stage MUST have a clone() method to create a copy of itself sharing or not the resources.
  // Each stage CAN have a merge() method to merge the output computed in each thread.
  virtual LASRalgorithm* clone() const { return nullptr; }; // = 0
  virtual void merge(const LASRalgorithm* other) { return; };

  void set_ncpu(int ncpu) { this->ncpu = ncpu; }
  void set_verbose(bool verbose) { this->verbose = verbose; };
  void set_uid(std::string s) { uid = s; };
  void set_progress(Progress* progress) { this->progress = progress; };
  void set_chunk(double xmin, double ymin, double xmax, double ymax) { this->xmin = xmin; this->ymin = ymin; this->xmax = xmax; this->ymax = ymax; };
  std::string get_uid() const { return uid; };

  // The default method consist in returning the string 'ofile'.
  #ifdef USING_R
  virtual SEXP to_R();
  #endif

protected:
  int ncpu;
  double xmin;
  double ymin;
  double xmax;
  double ymax;
  bool circular;
  bool verbose;
  std::string ifile;
  std::string ofile;
  std::string uid;
  LASfilter lasfilter;
  Progress* progress;

#ifdef USING_R
  int nsexpprotected;
#endif
};

class LASRalgorithmWriter : public LASRalgorithm
{
public:
  #ifdef USING_R
  SEXP to_R() override;
  #endif

protected:
  bool merged;
  std::string template_filename;
  std::vector<std::string> written;
};

class LASRalgorithmRaster : public LASRalgorithmWriter
{
public:
  LASRalgorithmRaster();
  ~LASRalgorithmRaster() override;
  bool set_chunk(const Chunk& chunk) override;
  void set_input_file_name(std::string file) override;
  void set_output_file(std::string file) override;
  bool set_crs(int epsg) override;
  bool set_crs(std::string wkt) override;
  bool write() override;
  //void clear(bool last) override;
  Raster& get_raster() { return raster; };

protected:
  Raster raster;
};

class LASRalgorithmVector : public LASRalgorithmWriter
{
public:
  LASRalgorithmVector();
  ~LASRalgorithmVector() override;
  bool set_chunk(const Chunk& chunk) override;
  void set_input_file_name(std::string file) override;
  void set_output_file(std::string file) override;
  bool set_crs(int epsg) override;
  bool set_crs(std::string wkt) override;
  //void clear(bool last) override;
  Vector& get_vector() { return vector; };

protected:
  Vector vector;
};

#endif

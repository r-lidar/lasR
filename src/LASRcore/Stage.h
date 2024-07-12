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
#include <map>

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
#include "CRS.h"
#include "error.h"
#include "print.h"

class Stage
{
public:
  Stage();
  Stage(const Stage& other); // copy constructor is for multi-threading
  virtual ~Stage() = 0;
  virtual bool process() { return true; };
  virtual bool process(LASheader*& header) { return true; };
  virtual bool process(LASpoint*& p) { return true; };
  virtual bool process(LAS*& las) { return true; };
  virtual bool process(LAScatalog*& las) { return true; };
  virtual bool break_pipeline() { return false; };
  virtual bool write() { return true; };
  virtual void clear(bool last = false) { return; };
  virtual bool set_chunk(const Chunk& chunk);
  virtual void set_crs(const CRS& crs) { this->crs = crs; };
  virtual bool set_output_file(const std::string& file) { ofile = file; return true; };
  virtual bool set_input_file_name(const std::string& file) { return true; };
  virtual void set_header(LASheader*& header) { return; };
  virtual bool is_streamable() const { return false; };
  virtual bool is_parallelizable() const { return true; }; // concurrent-files
  virtual bool is_parallelized() const { return false; };  // concurrent-points
  virtual bool use_rcapi() const { return false; };
  virtual double need_buffer() const { return 0; };
  virtual bool need_points() const { return true; };

  virtual std::string get_name() const = 0;

  void set_ncpu(int ncpu) { this->ncpu = ncpu; }
  void set_ncpu_concurrent_files(int ncpu) { this->ncpu_concurrent_files = ncpu; }
  void set_verbose(bool verbose) { this->verbose = verbose; };
  void set_uid(std::string s) { uid = s; };
  void set_filter(const std::string& f);
  void set_progress(Progress* progress) { this->progress = progress; };
  void set_chunk(double xmin, double ymin, double xmax, double ymax) { this->xmin = xmin; this->ymin = ymin; this->xmax = xmax; this->ymax = ymax; };
  std::string get_uid() const { return uid; };
  const std::map<std::string, Stage*>& get_connection() { return connections; };
  CRS get_crs() const { return crs; };

  bool is_valid_pointer(void*) const;

  // The default method consists in returning the string 'ofile'.
  #ifdef USING_R
  virtual SEXP to_R();
  #endif

  // For multi-threading when processing several files in parallel.
  // Each stage MUST have a clone() method to create a copy of itself sharing or not the resources.
  // Each stage CAN have a merge() method to merge the output computed in each thread.
  // Each stage must update the pointers to the other stages it depends on. This performed
  // in the copy constructor of the pipeline.
  // Each stage can have a sort member call by the pipeline to reorder and preserve order when computing in parallel
  virtual Stage* clone() const = 0;
  virtual void merge(const Stage* other) { return; };
  virtual void sort(const std::vector<int>& order) { return; };
  void update_connection(Stage* stage);


protected:
  void set_connection(Stage* stage);

protected:
  int ncpu;
  int ncpu_concurrent_files;
  double xmin;
  double ymin;
  double xmax;
  double ymax;
  bool circular;
  bool verbose;
  CRS crs;
  std::string ifile;
  std::string ofile;
  std::string uid;
  std::string filter;
  LASfilter lasfilter;
  Progress* progress;
  std::map<std::string, Stage*> connections;

#ifdef USING_R
  int nsexpprotected;
#endif
};

class StageWriter : public Stage
{
public:
  StageWriter();
  StageWriter(const StageWriter& other);
  void merge(const Stage* other) override;
  void sort(const std::vector<int>& order) override;

  #ifdef USING_R
  SEXP to_R() override;
  #endif

protected:
  bool merged;
  std::string template_filename;
  std::vector<std::string> written;
};

class StageRaster : public StageWriter
{
public:
  StageRaster();
  StageRaster(const StageRaster& other);
  ~StageRaster() override;
  bool set_chunk(const Chunk& chunk) override;
  void set_crs(const CRS& crs) override;
  bool set_input_file_name(const std::string& file) override;
  bool set_output_file(const std::string& file) override;
  bool write() override;
  //void clear(bool last) override;
  const Raster& get_raster() { return raster; };

protected:
  Raster raster;
};

class StageVector : public StageWriter
{
public:
  StageVector();
  StageVector(const StageVector& other);
  ~StageVector() override;
  bool set_chunk(const Chunk& chunk) override;
  void set_crs(const CRS& crs) override;
  bool set_input_file_name(const std::string& file) override;
  bool set_output_file(const std::string& file) override;
  //void clear(bool last) override;
  Vector& get_vector() { return vector; };

protected:
  Vector vector;
};

#endif

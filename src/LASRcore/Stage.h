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
#include <list>

// lasR
#include "LAS.h"
#include "LAScatalog.h"
#include "Raster.h"
#include "Vector.h"
#include "Progress.h"
#include "CRS.h"
#include "error.h"
#include "print.h"
#include "PointFilter.h"

// JSON parser
#include "nlohmann/json.hpp"

/* The order the functions are called
 *
 * In Pipeline::parser() to build the pipeline
 *  1. Constructor
 *  2. set_uid()
 *  3. set_ncpu()
 *  4. set_verbose()
 *  5. set_extent()
 *  6. connect()
 *  7. set_parameters()
 *  8. set_crs()
 *  9. get_crs()
 *  10. set_filter()
 *  11. set_output_file()
 * In the copy constructor of a Pipeline
 *  12. clone()
 * In Pipeline::set_chunk()
 *  13. set_chunk()
 *  14. break_pipeline()
 *  15. set_input_file_name()
 * In Pipeline::run()
 *  16. break_pipeline()
 *  17. process()
 *  18. process(LASheader)
 *  19. set_header()
 *  20. process(LAS)
 *  21. write()
 *  22. reset_filter()
 *  23. clear()
 *  24. clean()
 * Pipeline::clear(true)
 *  23. clear(true)
 * Pipeline::merge()
 *  26. merge()
 * Pipeline::sort()
 *  27. sort()
 * Pipeline::to_R()
 *  28. to_R();
 */

class Stage
{
public:
  Stage();
  Stage(const Stage& other); // copy constructor is for multi-threading
  virtual ~Stage() = 0;
  virtual bool process() { return true; };
  virtual bool process(Header*& header) { return true; };
  virtual bool process(Point*& p) { return true; };
  virtual bool process(LAS*& las) { return true; };
  virtual bool process(LAScatalog*& las) { return true; };
  virtual bool break_pipeline() { return false; };
  virtual bool write() { return true; };
  virtual void clear(bool last = false) { return; };
  virtual void set_crs(const CRS& crs) { this->crs = crs; };
  virtual bool set_output_file(const std::string& file) { ofile = file; return true; };
  virtual bool set_input_file_name(const std::string& file) { return true; };
  virtual void set_header(Header*& header) { return; };
  virtual bool set_chunk(Chunk& chunk);
  virtual bool set_parameters(const nlohmann::json&) { return true; };
  virtual bool is_streamable() const { return false; };
  virtual bool is_parallelizable() const { return true; }; // concurrent-files
  virtual bool is_parallelized() const { return false; };  // concurrent-points
  virtual bool use_rcapi() const { return false; };
  virtual double need_buffer() const { return 0; };
  virtual bool need_points() const { return true; };
  virtual void get_extent(double& xmin, double& ymin, double& xmax, double& ymax) { return; };


  virtual bool connect(const std::list<std::unique_ptr<Stage>>&, const std::string& uid) { return true; };

  //virtual void convert_units() { return; };

  virtual std::string get_name() const = 0;

  void set_extent(double xmin, double ymin, double xmax, double ymax) { this->xmin = xmin; this->ymin = ymin; this->xmax = xmax; this->ymax = ymax; };
  void set_ncpu(int ncpu) { this->ncpu = ncpu; }
  void set_ncpu_concurrent_files(int ncpu) { this->ncpu_concurrent_files = ncpu; }
  void set_verbose(bool verbose) { this->verbose = verbose; };
  void set_uid(std::string s) { uid = s; };
  void set_filter(const std::vector<std::string>& f);
  //void set_filter(const std::string& f);
  void set_progress(Progress* progress) { this->progress = progress; };
  void set_chunk(double xmin, double ymin, double xmax, double ymax) { this->xmin = xmin; this->ymin = ymin; this->xmax = xmax; this->ymax = ymax; };
  void reset_filter() { };

  std::string get_uid() const { return uid; };
  const std::map<std::string, Stage*>& get_connection() { return connections; };
  CRS get_crs() const { return crs; };

  bool is_valid_pointer(void*) const;

  // The default method consists in returning the string 'ofile'.
  #ifdef USING_R
  virtual SEXP to_R();
  #endif

  virtual nlohmann::json to_json() const;

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
  Stage* search_connection(const std::list<std::unique_ptr<Stage>>&, const std::string& uid);

  double convert_units(double) const;

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
  std::vector<std::string> filters;
  PointFilter pointfilter;
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

  nlohmann::json to_json() const override;

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
  bool set_chunk(Chunk& chunk) override;
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
  bool set_chunk(Chunk& chunk) override;
  void set_crs(const CRS& crs) override;
  bool set_input_file_name(const std::string& file) override;
  bool set_output_file(const std::string& file) override;
  //void clear(bool last) override;
  Vector& get_vector() { return vector; };

protected:
  Vector vector;
};

class StageMatrix : public Stage
{
public:
  StageMatrix();
  StageMatrix(const StageMatrix& other);
  void merge(const Stage* other) override;
  void transform(double& x, double& y, double& z);

  #ifdef USING_R
    SEXP to_R() override;
  #endif

  nlohmann::json to_json() const override;

protected:
  double matrix[4][4];
};

template <typename T>
std::vector<T> get_vector(const nlohmann::json& element)
{
  std::vector<T> result;

  if (element.is_array())
  {
    for (const auto& item : element)
    {
      result.push_back(item.get<T>());
    }
  }
  else if (element.is_null()) // Handle the case where the element is null
  {
    // No action needed; result is already an empty vector
  }
  else
  {
    result.push_back(element.get<T>());
  }

  return result;
}

#ifdef USING_R
SEXP string_address_to_sexp(const std::string& addr);
#endif

#endif

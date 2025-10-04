#ifndef LASRENGINE_H
#define LASRENGINE_H

// R
#ifdef USING_R
#define R_NO_REMAP 1
#include <R.h>
#include <Rinternals.h>
#endif

// LASR
#include "Stage.h"
#include "Profiler.h"

// STL
#include <chrono>
#include <memory>
#include <list>

// JSON parser
#include "nlohmann/json.hpp"

class LAS;
class LASpoint;
class LASheader;
class Progress;

class Engine
{
public:
  Engine();
  Engine(const Engine& other);
  ~Engine();
  bool parse(const nlohmann::json&, bool progress = false); // implemented in parser.cpp
  bool pre_run();
  bool run();
  void merge(const Engine& other);
  void clear(bool last = false);
  bool is_parallelizable() const;
  bool is_parallelized() const;
  bool is_streamable() const;
  bool use_rcapi() const;
  double need_buffer();
  bool need_points() const;
  bool set_chunk(Chunk& chunk);
  void set_ncpu(int ncpu);
  void set_ncpu_concurrent_files(int ncpu);
  void set_verbose(bool verbose);
  void sort();
  void show_profiling(const std::string& path);
  void set_progress(Progress* progress);
  FileCollection* get_catalog() const { return catalog.get(); };

#ifdef USING_R
  SEXP to_R();
#endif
  nlohmann::json to_json();

  Profiler profiler;

private:
  bool run_streamed();
  bool run_loaded();
  void clean();

private:
  int ncpu;
  bool parsed;
  bool verbose;
  bool streamable;
  bool parallelizable;
  bool read_payload;
  double buffer;
  double chunk_size;
  std::vector<int> order;

  PointCloud* las;                           // owned by this
  Point* point;                              // owned by las or by reader_las in streaming mode
  Header* header;                            // owned by las or by reader_las in streaming mode
  std::shared_ptr<FileCollection> catalog;   // owned by this and shared in cloned pipelines
  bool point_cloud_ownership_transfered;

  std::list<std::unique_ptr<Stage>> pipeline;
};

#endif
#ifndef LASRPIPELINE_H
#define LASRPIPELINE_H

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

class Pipeline
{
public:
  Pipeline();
  Pipeline(const Pipeline& other);
  ~Pipeline();
  bool parse(const nlohmann::json&, bool progress = false); // implemented in parser.cpp
  bool pre_run();
  bool run();
  void merge(const Pipeline& other);
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
  LAScatalog* get_catalog() const { return catalog.get(); };

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

  LAS* las;                             // owned by this
  LASpoint* point;                      // owned by LASreader and managed by las
  LASheader* header;                    // owned by LASreader and managed by las
  std::shared_ptr<LAScatalog> catalog;  // owned by this and shared in cloned pipelines

  std::list<std::unique_ptr<Stage>> pipeline;
};

#endif
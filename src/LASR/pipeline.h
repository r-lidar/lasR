#ifndef LASRPIPELINE_H
#define LASRPIPELINE_H

#ifdef USING_R
// R
#define R_NO_REMAP 1
#include <R.h>
#include <Rinternals.h>
#endif

// LASR
#include "lasralgorithm.h"

// STL
#include <memory>
#include <list>

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
    bool parse(const SEXP sexpargs, bool build_catalog = true, bool progress = false); // implemented in parser.cpp
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
    bool set_chunk(const Chunk& chunk);
    bool set_crs(int epsg);
    bool set_crs(std::string wkt);
    void set_ncpu(int ncpu);
    void set_verbose(bool verbose);
    void sort();
    //void show();
    void set_progress(Progress* progress);
    LAScatalog* get_catalog() const { return catalog.get(); };

    #ifdef USING_R
    SEXP to_R();
    #endif

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
    std::vector<int> order;

    LAS* las;                             // owned by this
    LASpoint* point;                      // owned by LASreader in stage reader_las or by las
    LASheader* header;                    // owned by LASreader in stage reader_las or by las
    std::shared_ptr<LAScatalog> catalog;  // owned by this and shared in cloned pipelines

    std::list<std::unique_ptr<LASRalgorithm>> pipeline;
};

#endif
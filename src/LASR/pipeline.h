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
    ~Pipeline();
    bool parse(const SEXP sexpargs, bool build_catalog = true, bool progress = false); // implemented in parser.cpp
    bool pre_run();
    bool run();
    void clear(bool last = false);
    bool is_streamable();
    double need_buffer();
    bool need_points();
    bool set_chunk(const Chunk& chunk);
    bool set_crs(int epsg);
    bool set_crs(std::string wkt);
    void set_ncpu(int ncpu);
    void set_verbose(bool verbose);
    //void set_buffer(double buffer);
    void set_progress(Progress* progress);
    LAScatalog* get_catalog() const { return catalog; };

    #ifdef USING_R
    SEXP to_R();
    #endif

private:
  void clean();

  private:
    int ncpu;
    bool parsed;
    bool verbose;
    bool streamable;
    bool read_payload;
    double buffer;

    LAS* las;
    LASpoint* point;
    LASheader* header;
    LAScatalog* catalog;

    std::list<std::unique_ptr<LASRalgorithm>> pipeline;
};

#endif
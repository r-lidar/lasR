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
    std::string get_last_error() const { return last_error; };

    #ifdef USING_R
    SEXP to_R();
    #endif

private:
  bool process(LASheader*& header);
  bool process(LASpoint*& p);
  bool process(LAS*& las);
  bool process(LAScatalog*& catalog);
  bool write();
  void clean();
  void set_header(LASheader*& header);

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
    std::string last_error;
    std::list<std::unique_ptr<LASRalgorithm>> pipeline;
};

#endif
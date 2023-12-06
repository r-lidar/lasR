#ifndef LASRPIPELINE_H
#define LASRPIPELINE_H

#ifdef USING_R
// R
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
    bool parse(const SEXP sexpargs, bool build_catalog = true); // implemented in parser.cpp
    bool run();

    bool process(LASheader*& header);
    bool process(LASpoint*& p);
    bool process(LAS*& las);
    bool write();
    void clear(bool last = false);
    bool set_chunk(const Chunk& chunk);
    bool set_crs(int epsg);
    bool set_crs(std::string wkt);
    void set_header(LASheader*& header);
    bool is_streamable();
    double need_buffer();
    bool need_points();
    void set_filter(std::string f);
    void set_ncpu(int ncpu);
    void set_verbose(bool verbose);
    void set_buffer(double buffer);
    void set_progress(Progress* progress);
    void set_chunk(double xmin, double ymin, double xmax, double ymax);
    LAScatalog* get_catalog() { return catalog; };
    std::string get_last_error() { return last_error; };

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
    std::string last_error;
    std::list<std::shared_ptr<LASRalgorithm>> pipeline;
};

#endif
// R
#ifdef USING_R
#define R_NO_REMAP 1
#include <R.h>
#include <Rinternals.h>
#endif

#include "Rcompatibility.h"

// STL
#include <memory>
#include <vector>

// lasR
#include "openmp.h"
#include "error.h"
#include "pipeline.h"
#include "LAScatalog.h"

SEXP process(SEXP sexppipeline, SEXP sexpprogrss, SEXP sexpncpu, SEXP sexpverbose)
{
  int ncpu = Rf_asInteger(sexpncpu);
  bool progrss = Rf_asLogical(sexpprogrss);
  bool verbose = Rf_asLogical(sexpverbose);

  try
  {
    Pipeline pipeline;
    pipeline.set_verbose(verbose);
    pipeline.set_ncpu(1);

    if (!pipeline.parse(sexppipeline, true, progrss))
    {
      throw last_error;
    }

    LAScatalog* lascatalog = pipeline.get_catalog(); // the pipeline owns the catalog
    lascatalog->set_chunk_size(0);                   // currently only 0 is supported

    int n = lascatalog->get_number_chunks();
    if (ncpu > n) ncpu = n;

    if (verbose)
    {
      // # nocov start
      print("File processing options:\n");
      print("  Read points: %s\n", pipeline.need_points() ? "true" : "false");
      print("  Streamable: %s\n", pipeline.is_streamable() ? "true" : "false");
      print("  Buffer: %.1lf\n", pipeline.need_buffer());
      print("  Chunks: %d\n", n);
      print("\n");
      // # nocov end
    }

    // Initialize progress bars
    Progress progress;
    progress.set_prefix("Overall");
    progress.set_total(n);
    progress.set_display(progrss);
    progress.create_subprocess();
    pipeline.set_progress(&progress);

    // Pre-run processes the LAScatalog
    // write_vpc() is the only stage that processed the LAScatalog
    if (!pipeline.pre_run())
    {
      throw last_error;
    }

    progress.show();

    bool failure = false;
    #pragma omp parallel num_threads(ncpu)
    {
      try
      {
        // We need a copy of the pipeline. The copy constructor of the pipeline and stages
        // ensures that that shared resources are protected (such as connection to output files)
        // and private data are copied
        Pipeline pipeline_cpy(pipeline);

        #pragma omp for
        for (int i = 0 ; i < n ; ++i)
        {
          if (failure) continue;

          bool last_chunk = i+1 == n; // TODO:: need to be revised for parallel code

          // We query the chunk i (thread safe)
          Chunk chunk;
          if (!lascatalog->get_chunk(i, chunk))
          {
            failure = true;
            continue;
          }

          if (verbose)
          {
            print("Processing chunk %d/%d in thread %d: %s\n", i+1, n, omp_get_thread_num(), chunk.name.c_str()); // # nocov
          }

          // If the chunk is not flagged "process" it is a file that is only used as buffer
          // we can skip the processing
          if (!chunk.process)
          {
            progress.update(i+1, true);
            progress.show();
            if (verbose) print("Chunk %d is flagged for not being processed. Skipped.", i);
            continue;
          }

          // set_chunk() initialize the region we are working with which is a sub-part of the
          // overall processed region
          if (!pipeline_cpy.set_chunk(chunk))
          {
            failure = true;
            continue;
          }

          // run() does execute the pipeline. This is encapsulated but at the end each stage
          // is supposed to contains the data for the current chunk and optionally write the
          // result into a file
          if (!pipeline_cpy.run())
          {
            failure = true;
            continue;
          }

          // clear() is used by some stages to clean data between two chunks. This is useful
          // to clear an std::map like in the aggregate pipeline
          pipeline_cpy.clear(last_chunk);

          #pragma omp critical
          {
            progress.update(i+1, true);
            progress.show();
          }
        }

        // We have multiple pipelines that each process some chunks and each have a partial
        // output. We merge in the main pipeline
        #pragma omp critical
        {
          pipeline.merge(pipeline_cpy);
        }
      }
      catch (std::string e)
      {
        last_error = e;
        failure = true;
      }
    }

    if (failure)
    {
      throw last_error;
    }

    // We are no longer in the parallel region we can return to R by allocating safely
    // some R memory

    progress.done(true);

    return pipeline.to_R();
  }
  catch (std::string e)
  {
    SEXP res = PROTECT(Rf_allocVector(VECSXP, 2)) ;

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
    SET_STRING_ELT(names, 0, Rf_mkChar("message")) ;
    SET_STRING_ELT(names, 1, Rf_mkChar("call")) ;
    Rf_setAttrib(res, R_NamesSymbol, names);

    SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(classes, 0, Rf_mkChar("C++Error"));
    SET_STRING_ELT(classes, 1, Rf_mkChar("error"));
    SET_STRING_ELT(classes, 2, Rf_mkChar("condition"));
    Rf_setAttrib(res, R_ClassSymbol, classes);

    SEXP R_err = PROTECT(Rf_allocVector(STRSXP, 1));
    SEXP R_msg = PROTECT(Rf_mkChar(e.c_str()));
    SET_STRING_ELT(R_err, 0, R_msg);

    SET_VECTOR_ELT(res, 0, R_err);
    SET_VECTOR_ELT(res, 1, R_NilValue);

    UNPROTECT(5);
    return res;
  }
  catch(...)
  {
    // # nocov start
    SEXP res = PROTECT(Rf_allocVector(VECSXP, 2)) ;

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
    SET_STRING_ELT(names, 0, Rf_mkChar("message")) ;
    SET_STRING_ELT(names, 1, Rf_mkChar("call")) ;
    Rf_setAttrib(res, R_NamesSymbol, names);

    SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(classes, 0, Rf_mkChar("C++Error"));
    SET_STRING_ELT(classes, 1, Rf_mkChar("error"));
    SET_STRING_ELT(classes, 2, Rf_mkChar("condition"));
    Rf_setAttrib(res, R_ClassSymbol, classes);

    SEXP R_err = PROTECT(Rf_allocVector(STRSXP, 1));
    SEXP R_msg = PROTECT(Rf_mkChar("c++ exception (unknown reason)"));
    SET_STRING_ELT(R_err, 0, R_msg);

    SET_VECTOR_ELT(res, 0, R_err);
    SET_VECTOR_ELT(res, 1, R_NilValue);

    UNPROTECT(5);
    return res;
    // # nocov end
  }

  return R_NilValue;
}

#ifdef USING_R
SEXP get_pipeline_info(SEXP sexppipeline)
{
  try
  {
    Pipeline pipeline;
    if (!pipeline.parse(sexppipeline, false))
    {
      throw last_error;
    }
    bool is_streamable = pipeline.is_streamable();
    bool read_points = pipeline.need_points();
    double buffer = pipeline.need_buffer();

    SEXP ans =  PROTECT(Rf_allocVector(VECSXP, 3));
    SET_VECTOR_ELT(ans, 0, Rf_ScalarLogical(is_streamable));
    SET_VECTOR_ELT(ans, 1, Rf_ScalarLogical(read_points));
    SET_VECTOR_ELT(ans, 2, Rf_ScalarReal(buffer));

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(names, 0, Rf_mkChar("streamable"));
    SET_STRING_ELT(names, 1, Rf_mkChar("read_points"));
    SET_STRING_ELT(names, 2, Rf_mkChar("buffer"));
    Rf_setAttrib(ans, R_NamesSymbol, names);

    UNPROTECT(2);
    return ans;
  }
  catch (std::string e)
  {
    SEXP res = PROTECT(Rf_allocVector(VECSXP, 2)) ;

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 2));
    SET_STRING_ELT(names, 0, Rf_mkChar("message")) ;
    SET_STRING_ELT(names, 1, Rf_mkChar("call")) ;
    Rf_setAttrib(res, R_NamesSymbol, names);

    SEXP classes = PROTECT(Rf_allocVector(STRSXP, 3));
    SET_STRING_ELT(classes, 0, Rf_mkChar("C++Error"));
    SET_STRING_ELT(classes, 1, Rf_mkChar("error"));
    SET_STRING_ELT(classes, 2, Rf_mkChar("condition"));
    Rf_setAttrib(res, R_ClassSymbol, classes);

    SEXP R_err = PROTECT(Rf_allocVector(STRSXP, 1));
    SEXP R_msg = PROTECT(Rf_mkChar(e.c_str()));
    SET_STRING_ELT(R_err, 0, R_msg);

    SET_VECTOR_ELT(res, 0, R_err);
    SET_VECTOR_ELT(res, 1, R_NilValue);

    UNPROTECT(5);
    return res;
  }
  catch(...)
  {
    Rf_error("c++ exception (unknown reason)");
  }

  return R_NilValue;
}
#endif

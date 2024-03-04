// R
#ifdef USING_R
#define R_NO_REMAP 1
#include <R.h>
#include <Rinternals.h>
#define CSTACK_DEFNS 1
#include <Rinterface.h> // R_CStackLimit
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
#include "openmp.h"

#define SEQUENTIAL 1
#define CONCURRENTPOINTS 2
#define CONCURRENTFILES 3
#define NESTED 4

SEXP process(SEXP sexppipeline, SEXP sexpprogrss, SEXP sexpncpu, SEXP sexpmode, SEXP sexpverbose)
{
  int ncpu = Rf_asInteger(sexpncpu);
  int mode = Rf_asInteger(sexpmode);
  bool progrss = Rf_asLogical(sexpprogrss);
  bool verbose = Rf_asLogical(sexpverbose);

  // Check some multithreading stuff
  if (ncpu > available_threads())
  {
    warning("Number of cores requested %d but only %d available\n", ncpu, available_threads());
    ncpu = available_threads();
  }
  int ncpu_outer_loop = 1; // concurrent files
  int ncpu_inner_loops = 1; // concurrent points
  if (mode == CONCURRENTPOINTS) ncpu_inner_loops = ncpu;
  if (mode == CONCURRENTFILES) ncpu_outer_loop = ncpu;
  if (mode == NESTED)
  {
    ncpu_outer_loop = ncpu;
    ncpu_inner_loops = INTEGER(sexpncpu)[1];
  }
  if (ncpu_outer_loop > 1 && ncpu_inner_loops > 1) omp_set_max_active_levels(2); // nested

  uintptr_t original_CStackLimit = R_CStackLimit;

  try
  {
    Pipeline pipeline;

    if (!pipeline.parse(sexppipeline, true, progrss))
    {
      throw last_error;
    }

    bool use_rcapi = pipeline.use_rcapi();
    bool is_parallelized = pipeline.is_parallelized();      // concurrent-points
    bool is_parallelizable = pipeline.is_parallelizable();  // concurrent-files

    LAScatalog* lascatalog = pipeline.get_catalog(); // the pipeline owns the catalog
    lascatalog->set_chunk_size(0);                   // currently only 0 is supported
    int n = lascatalog->get_number_chunks();

    // Check some multi-threading stuff
    if (!is_parallelized && ncpu_inner_loops > 1)
    {
      ncpu_inner_loops = 1;
    }
    if (!is_parallelizable && ncpu_outer_loop > 1)
    {
      ncpu_inner_loops = ncpu_outer_loop;
      ncpu_outer_loop = 1;
      warning("This pipeline is not parallizable using 'concurrent-files' strategy.\n");
    }
    if (ncpu_outer_loop > n) ncpu_outer_loop = n;
    if (pipeline.use_rcapi() && ncpu_outer_loop > 1)
    {
      // The R's C stack is now unprotected — the work with R C API becomes more dangerous
      // but we can run parallel stuff without strange problems like: C stack usage is too close to the limit
      // It is supposed to be safe because every single call to the R's C API is protected in a critical section
      // https://stats.blogoverflow.com/2011/08/using-openmp-ized-c-code-with-r/
      // https://stat.ethz.ch/pipermail/r-devel/2007-June/046207.html
      R_CStackLimit=(uintptr_t)-1;
    }

    pipeline.set_verbose(verbose);
    pipeline.set_ncpu(ncpu_inner_loops);

    if (verbose)
    {
      // # nocov start
      print("File processing options:\n");
      print("  Read points: %s\n", pipeline.need_points() ? "true" : "false");
      print("  Streamable: %s\n", pipeline.is_streamable() ? "true" : "false");
      print("  Buffer: %.1lf\n", pipeline.need_buffer());
      print("  Concurrent files: %d\n", ncpu_outer_loop);
      print("  Concurrent points: %d\n", ncpu_inner_loops);
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
    int k = 0;
    #pragma omp parallel num_threads(ncpu_outer_loop)
    {
      try
      {
        // We need a copy of the pipeline. The copy constructor of the pipeline and stages
        // ensure that shared resources are protected (such as connection to output files)
        // and private data are copied.
        Pipeline private_pipeline(pipeline);

        #pragma omp for
        for (int i = 0 ; i < n ; ++i)
        {
          // We cannot exit a parallel loop easily. Instead we can rather run the loop until the end
          // skipping the processing
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
            #pragma omp critical
            {
              k++;
              progress.update(k, true);
              progress.show();
              if (verbose) print("Chunk %d is flagged for not being processed. Skipped.", i);
            }

            continue;
          }

          // set_chunk() initialize the region we are working with which is a sub-part of the
          // overall processed region
          if (!private_pipeline.set_chunk(chunk))
          {
            failure = true;
            continue;
          }

          // run() does execute the pipeline. This is encapsulated but at the end each stage
          // is supposed to contains the data for the current chunk and optionally write the
          // result into a file
          if (!private_pipeline.run())
          {
            failure = true;
            continue;
          }

          #pragma omp critical
          {
            k++;
            progress.update(k, true);
            progress.show();
          }
        }

        // We are outside the main loop. We can clear the pipeline with last = true;
        private_pipeline.clear(true);

        // We have multiple pipelines that each process some chunks and each have a partial
        // output. We reduce in the main pipeline
        #pragma omp critical
        {
          if (!failure)
          {
            pipeline.merge(private_pipeline);
          }
        }
      }
      catch (std::string e)
      {
        last_error = e;
        failure = true;
      }
    }

    // We are no longer in the parallel region we can return to R by allocating safely
    // some R memory

    R_CStackLimit=original_CStackLimit;

    if (failure)
    {
      throw last_error;
    }

    progress.done(true);

    return pipeline.to_R();
  }
  catch (std::string e)
  {
    R_CStackLimit=original_CStackLimit;

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
    R_CStackLimit=original_CStackLimit;

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
    bool is_parallelizable = pipeline.is_parallelizable();
    bool is_parallelized = pipeline.is_parallelized();
    bool read_points = pipeline.need_points();
    bool use_rcapi = pipeline.use_rcapi();
    double buffer = pipeline.need_buffer();

    SEXP ans =  PROTECT(Rf_allocVector(VECSXP, 6));
    SET_VECTOR_ELT(ans, 0, Rf_ScalarLogical(is_streamable));
    SET_VECTOR_ELT(ans, 1, Rf_ScalarLogical(read_points));
    SET_VECTOR_ELT(ans, 2, Rf_ScalarReal(buffer));
    SET_VECTOR_ELT(ans, 3, Rf_ScalarLogical(is_parallelizable));
    SET_VECTOR_ELT(ans, 4, Rf_ScalarLogical(is_parallelized));
    SET_VECTOR_ELT(ans, 5, Rf_ScalarLogical(use_rcapi));

    SEXP names = PROTECT(Rf_allocVector(STRSXP, 6));
    SET_STRING_ELT(names, 0, Rf_mkChar("streamable"));
    SET_STRING_ELT(names, 1, Rf_mkChar("read_points"));
    SET_STRING_ELT(names, 2, Rf_mkChar("buffer"));
    SET_STRING_ELT(names, 3, Rf_mkChar("parallelizable"));
    SET_STRING_ELT(names, 4, Rf_mkChar("parallelized"));
    SET_STRING_ELT(names, 5, Rf_mkChar("R_API"));
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

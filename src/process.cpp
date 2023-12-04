// R
#ifdef USING_R
#include <R.h>
#include <Rinternals.h>
#endif

#include "Rcompatibility.h"

// STL
#include <memory>
#include <vector>

// lasR
#include "pipeline.h"
#include "LAScatalog.h"

SEXP process(SEXP sexppipeline, SEXP sexpprogrss, SEXP sexpncpu, SEXP sexpverbose)
{
  try
  {
    SEXP default_return = R_NilValue;

    #ifdef USING_R
    // convert R object
    int ncpu = Rf_asInteger(sexpncpu);
    bool progrss = Rf_asLogical(sexpprogrss);
    bool verbose = Rf_asLogical(sexpverbose);
    #endif

    Pipeline pipeline;
    pipeline.set_verbose(verbose);
    pipeline.set_ncpu(ncpu);

    if (!pipeline.parse(sexppipeline))
    {
     throw pipeline.get_last_error();
    }

    LAScatalog* lascatalog = pipeline.get_catalog(); // the pipeline owns the catalog
    lascatalog->check_spatial_index();  // prints a warning if no spatial index
    lascatalog->set_chunk_size(0);

    int n = lascatalog->get_number_chunks();

    if (verbose)
    {
      // # nocov start
      print("File processing options:\n");
      print(" Read points: %s\n", pipeline.need_points() ? "true" : "false");
      print(" Streamable: %s\n", pipeline.is_streamable() ? "true" : "false");
      print(" Buffer: %.1lf\n", pipeline.need_buffer());
      print(" Chunks: %d\n", n);
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

    for (int i = 0 ; i < n ; ++i)
    {
      bool last_chunk = i+1 == n;

      Chunk chunk;
      if (!lascatalog->get_chunk(i, chunk))
      {
        throw lascatalog->last_error.c_str();
      }

      if (verbose)
      {
        print("Processing chunk %d/%d: %s\n", i+1, n, chunk.name.c_str()); // # nocov
      }

      if (!pipeline.set_chunk(chunk))
      {
        throw pipeline.get_last_error();
      }

      if (!pipeline.run())
      {
        throw pipeline.get_last_error();
      }

      pipeline.clear(last_chunk);

      // Progress bar
      progress.update(i, true);
      progress.show();
    }

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
    Rf_error("c++ exception (unknown reason)");
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
      throw pipeline.get_last_error();
    }
    bool is_streamable = pipeline.is_streamable();
    bool read_points = pipeline.need_points();
    double buffer = pipeline.need_buffer();

    SEXP ans =  PROTECT(allocVector(VECSXP, 3));
    SET_VECTOR_ELT(ans, 0, ScalarLogical(is_streamable));
    SET_VECTOR_ELT(ans, 1, ScalarLogical(read_points));
    SET_VECTOR_ELT(ans, 2, ScalarReal(buffer));

    SEXP names = PROTECT(allocVector(STRSXP, 3));
    SET_STRING_ELT(names, 0, mkChar("streamable"));
    SET_STRING_ELT(names, 1, mkChar("read_points"));
    SET_STRING_ELT(names, 2, mkChar("buffer"));
    setAttrib(ans, R_NamesSymbol, names);

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

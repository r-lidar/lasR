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
  bool success = false;
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
    error("%s", pipeline.get_last_error().c_str());
    return default_return;
  }

  LAScatalog* lascatalog = pipeline.get_catalog(); // the pipeline owns the catalog
  lascatalog->check_spatial_index();  // prints a warning if no spatial index
  lascatalog->set_chunk_size(0);

  int n = lascatalog->get_number_chunks();

  if (verbose)
  {
    print("File processing options:\n");
    print(" Read points: %s\n", pipeline.need_points() ? "true" : "false");
    print(" Streamable: %s\n", pipeline.is_streamable() ? "true" : "false");
    print(" Buffer: %.1lf\n", pipeline.need_buffer());
    print("\n");
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
      error("%s", lascatalog->last_error.c_str());
      return default_return;
    }

    if (verbose)
    {
      print("Processing chunk %d/%d: %s\n", i+1, n, chunk.name.c_str());
    }

    if (!pipeline.set_chunk(chunk))
    {
      error("%s", pipeline.get_last_error().c_str());
      return default_return;
    }

    if (!pipeline.run())
    {
      error("%s", pipeline.get_last_error().c_str());
      return default_return;
    }

    pipeline.clear(last_chunk);

    // Progress bar
    progress.update(i, true);
    progress.show();
  }

  progress.done(true);

  #ifdef USING_R
  return pipeline.to_R();
  #else
  return default_return;
  #endif
}

#ifdef USING_R
SEXP get_pipeline_info(SEXP sexppipeline)
{
  Pipeline pipeline;
  if (!pipeline.parse(sexppipeline, false))
  {
    error("%s", pipeline.get_last_error().c_str());
    return R_NilValue;
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
#endif

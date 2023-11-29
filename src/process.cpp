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

SEXP process(SEXP sexppipeline, SEXP sexpfiles, SEXP sexpbuffer, SEXP sexpprogrss, SEXP sexpncpu, SEXP sexpverbose)
{
  bool success = false;
  SEXP default_return = R_NilValue;

  #ifdef USING_R
  // convert R object
  int ncpu = Rf_asInteger(sexpncpu);
  bool progrss = Rf_asLogical(sexpprogrss);
  bool verbose = Rf_asLogical(sexpverbose);
  double buffer = Rf_asReal(sexpbuffer);
  std::vector<std::string> files;
  for (auto i = 0; i < Rf_length(sexpfiles); i++)
  {
    const char* file = CHAR(STRING_ELT(sexpfiles, i));
    files.push_back(std::string(file));
  }
  #endif

  Pipeline pipeline;
  pipeline.set_verbose(verbose);
  pipeline.set_buffer(buffer);
  pipeline.set_ncpu(ncpu);

  LAScatalog lascatalog;
  for (std::string& file : files)
  {
    success = lascatalog.add_file(file);
    if (!success) { error("%s", lascatalog.last_error.c_str()); return default_return; }
  }

  lascatalog.check_spatial_index();  // prints a warning if no spatial index

  success = pipeline.parse(sexppipeline, lascatalog);
  if (!success) { error(pipeline.get_last_error().c_str()); return default_return; }

  lascatalog.set_chunk_size(0);
  lascatalog.set_buffer(pipeline.need_buffer());

  int n = lascatalog.get_number_chunks();

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

  for (int i = 0 ; i < n ; ++i)
  {
    bool last_chunk = i+1 == n;

    Chunk chunk;
    success = lascatalog.get_chunk(i, chunk);
    if (!success) { error("%s", lascatalog.last_error.c_str()); return default_return; }

    if (verbose) print("Processing chunk %d/%d: %s\n", i+1, n, chunk.name.c_str());

    success = pipeline.set_chunk(chunk);
    if (!success) { error(pipeline.get_last_error().c_str()); return default_return; }

    pipeline.set_input_file(chunk.main_file); // Used only by writelax
    pipeline.set_input_file_name(chunk.name);
    pipeline.set_progress(&progress);
    pipeline.set_crs(lascatalog.epsg);
    pipeline.set_crs(lascatalog.wkt);

    success = pipeline.run();
    if (!success) { error(pipeline.get_last_error().c_str()); return default_return; }

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

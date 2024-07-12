// R
#ifdef USING_R
  #define STRICT_R_HEADERS
  #define R_NO_REMAP 1
  #include <R.h>
  #include <Rinternals.h>

  /*#ifndef _WIN32
    #define CSTACK_DEFNS 1
    #include <Rinterface.h> // R_CStackLimit
  #else
    extern __declspec(dllimport) uintptr_t R_CStackLimit; // C stack limit
  #endif*/
#endif

#include <memory>
#include <vector>
#include <iostream>
#include <fstream>

#include "openmp.h"
#include "error.h"
#include "print.h"

#include "pipeline.h"
#include "LAScatalog.h"

#include "DrawflowParser.h"
#include "nlohmann/json.hpp"

#ifdef USING_R
SEXP process(SEXP sexp_config_file)
{
  std::string config_file = std::string(CHAR(STRING_ELT(sexp_config_file, 0)));
#else
bool process(const std::string& config_file)
{
#endif

  // Open the JSON file
  std::ifstream fjson(config_file);
  if (!fjson.is_open())
  {
    #ifdef USING_R
      return make_R_error("Could not open the json file containing the pipeline");
    #else
      eprint("Could not open the json file containing the pipeline");
      return false;
    #endif
  }

  nlohmann::json json;
  fjson >> json;

  // The json file is maybe a file produce by Drawflow. It must be converted into something
  // understandable by lasR
  if (json.contains("drawflow"))
  {
    try
    {
      json = DrawflowParser::parse(json);
    }
    catch (std::exception& e)
    {
      #ifdef USING_R
        return make_R_error(e.what());
      #else
        eprint(e.what());
        return false;
      #endif
    }
  }

  // The JSON file is made of the processing options and the pipeline
  nlohmann::json processing_options = json["processing"];
  nlohmann::json json_pipeline = json["pipeline"];

  // Parse the processing options
  std::vector<int> ncpu = get_vector<int>(processing_options["ncores"]);
  if (ncpu.size() == 0) ncpu.push_back(std::ceil((float)omp_get_num_threads()/2));
  std::string strategy = processing_options.value("strategy", "concurrent-points");
  bool progrss = processing_options.value("progress", true);
  bool verbose = processing_options.value("verbose", false);
  double chunk_size = processing_options.value("chunk", 0);
  std::string fprofiling = processing_options.value("profiling", "");
  std::string async_communication_file = processing_options.value("async_communication_file", "/tmp/com0.txt");

  // build_catalog() has been added at R level because there are some subtleties to handle LAS and LAScatalog
  // object from lidR. If build_catalog is missing, add it because we are using an API that is not R
  if (json_pipeline.empty() || json_pipeline[0]["algoname"] != "build_catalog")
  {
    nlohmann::json build_catalog = {
      {"algoname", "build_catalog"},
      {"files", processing_options["files"]},
      {"buffer", processing_options.value("buffer", 0)},
      {"uid", "6275696c645f636174616c6f67"},
      {"output", ""},
      {"filter", ""}
    };
    json_pipeline.insert(json_pipeline.begin(), build_catalog);
  }

  std::cout << std::setw(2) << json_pipeline << std::endl;
  return R_NilValue;

  // Check some multithreading stuff
  if (ncpu[0] > available_threads())
  {
    warning("Number of cores requested %d but only %d available\n", ncpu[0], available_threads());
    ncpu[0] = available_threads();
  }
  int ncpu_outer_loop = 1; // concurrent files
  int ncpu_inner_loops = 1; // concurrent points
  if (strategy == "concurrent-points") ncpu_inner_loops = ncpu[0];
  if (strategy == "concurrent-files") ncpu_outer_loop = ncpu[0];
  if (strategy == "nested")
  {
    if (ncpu.size() < 2) throw "Using nested strategy requires an array of two numbers in 'ncores'";

    ncpu_outer_loop = ncpu[0];
    ncpu_inner_loops = ncpu[1];
  }
  if (ncpu_outer_loop > 1 && ncpu_inner_loops > 1) omp_set_max_active_levels(2); // nested

  //#ifdef USING_R
  //uintptr_t original_CStackLimit = R_CStackLimit;
  //#endif

  try
  {
    Pipeline pipeline;

    if (!pipeline.parse(json_pipeline, progrss))
    {
      throw last_error;
    }

    bool use_rcapi = pipeline.use_rcapi();
    bool is_parallelized = pipeline.is_parallelized();      // concurrent-points
    bool is_parallelizable = pipeline.is_parallelizable();  // concurrent-files

    LAScatalog* lascatalog = pipeline.get_catalog(); // the pipeline owns the catalog

    if (!lascatalog->set_chunk_size(chunk_size))
    {
      throw last_error;
    }

    int n = lascatalog->get_number_chunks();

    // Check some multi-threading stuff
    if (ncpu_outer_loop > n) ncpu_outer_loop = n;
    if (!is_parallelized && ncpu_inner_loops > 1) ncpu_inner_loops = 1;
    if (!is_parallelizable && ncpu_outer_loop > 1)
    {
      ncpu_inner_loops = ncpu_outer_loop;
      ncpu_outer_loop = 1;
      warning("This pipeline is not parallizable using 'concurrent-files' strategy.\n");
    }

    if (use_rcapi && ncpu_outer_loop > 1)
    {
      // The R's C stack is now unprotected — the work with R C API becomes more dangerous
      // but we can run parallel stuff without strange problems like: C stack usage is too close to the limit
      // It is supposed to be safe because every single call to the R's C API is protected in a critical section
      // https://stats.blogoverflow.com/2011/08/using-openmp-ized-c-code-with-r/
      // https://stat.ethz.ch/pipermail/r-devel/2007-June/046207.html
      //R_CStackLimit=(uintptr_t)-1;
      ncpu_inner_loops = ncpu_outer_loop;
      ncpu_outer_loop = 1;
      warning("This pipeline is not parallizable using 'concurrent-files' strategy because of stage(s) that imply injected R code\n");
    }

    pipeline.set_verbose(verbose);
    pipeline.set_ncpu(ncpu_inner_loops);
    pipeline.set_ncpu_concurrent_files(ncpu_outer_loop);

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
    progress.set_ncpu(ncpu_outer_loop);
    progress.create_subprocess();
    progress.set_async_message_file(async_communication_file);

    pipeline.set_progress(&progress);

    // Pre-run processes the LAScatalog
    // write_vpc() and write_lax() are the only stage that can processed the LAScatalog
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

        #pragma omp for schedule(dynamic)
        for (int i = 0 ; i < n ; ++i)
        {
          // We cannot exit a parallel loop easily. Instead we can rather run the loop until the end
          // skipping the processing
          if (failure) continue;
          if (progress.interrupted()) continue;

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

        // We have multiple pipelines and each processed some chunks and each have a partial
        // output. We reduce in the main pipeline. To preserve the ordering of the output we
        // well call sort() outside the paraellel region
        #pragma omp critical
        {
          pipeline.merge(private_pipeline);
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
    //#ifdef USING_R
    //R_CStackLimit = original_CStackLimit;
    //#endif

    progress.done(true);

    if (failure)
    {
      throw last_error;
    }

    pipeline.sort();

    pipeline.profiler.write(fprofiling);

#ifdef USING_R
    return pipeline.to_R();
#else
    return true;
#endif
  }
  catch (std::string e)
  {
    #ifdef USING_R
      //R_CStackLimit = original_CStackLimit;
      return make_R_error(e.c_str());
    #else
      eprint(e.c_str());
      return false;
    #endif
  }
  catch(...)
  {
    // # nocov start
    #ifdef USING_R
      //R_CStackLimit = original_CStackLimit;
      return make_R_error("c++ exception (unknown reason)");
    #else
      eprint("c++ exception (unknown reason)");
      return false;
    #endif
    // # nocov end
  }

  #ifdef USING_R
    return R_NilValue;
  #else
    return false;
  #endif

}

#ifdef USING_R
SEXP get_pipeline_info(SEXP sexp_config_file)
{
  std::string config_file = std::string(CHAR(STRING_ELT(sexp_config_file, 0)));

  try
  {
    std::ifstream fjson(config_file);
    if (!fjson.is_open())
    {
      last_error = "Could not open the json file containing the pipeline";
      throw last_error;
    }
    nlohmann::json json;
    fjson >> json;

    nlohmann::json json_pipeline = json["pipeline"];


    Pipeline pipeline;
    if (!pipeline.parse(json_pipeline))
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
    return make_R_error(e.c_str());
  }
  catch(...)
  {
    return make_R_error("c++ exception (unknown reason)"); // # nocov
  }

  return R_NilValue; // # nocov
}
#endif

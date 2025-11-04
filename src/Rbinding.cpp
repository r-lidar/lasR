#ifdef USING_R

#include <RcppCommon.h>

#include "api.h"
#include "openmp.h"

namespace Rcpp
{
  template <> SEXP wrap(const api::Pipeline& pipeline)
  {
    // Create a deep copy allocated on the heap to ensure the object survives in the R session
    // and expose a pointer
    api::Pipeline* s = new api::Pipeline(pipeline);
    SEXP extptr = R_MakeExternalPtr(static_cast<void*>(s), R_NilValue, R_NilValue);

    // Register the R method to delete the point with GC
    R_RegisterCFinalizerEx(extptr, [](SEXP p) {
      api::Pipeline* s = static_cast<api::Pipeline*>(R_ExternalPtrAddr(p));
      if (s) delete s;
    }, TRUE);

    // Assign a class to the pointer for S3 overload
    SEXP cls = PROTECT(Rf_mkString("PipelinePtr"));
    Rf_setAttrib(extptr, R_ClassSymbol, cls);
    UNPROTECT(1);

    return extptr;
  }
}

#include <Rcpp.h>

using namespace Rcpp;

// Generic tools that could be used by any API
RCPP_MODULE(utils)
{
  function("lasfilterusage", &api::lasfilterusage, "LASlib filter list");
  function("lastransformusage", &api::lastransformusage, "LASlib transform list");
  function("available_threads", &api::available_threads, "Number of usable cores");
  function("has_omp_support", &api::has_omp_support,  "Is lasr build with OpenMP support");
  function("is_indexed", &api::is_indexed, "The LAS files is spatially indexed");
  function("getAvailableRAM", &api::getAvailableRAM, "FreeRAM");
  function("getTotalRAM", &api::getTotalRAM, "Total RAM");
}

// Generic stages that could be used by any API
RCPP_MODULE(stages)
{
  // STAGES
  function("add_attribute", &api::add_attribute, "Add an attribute to the point cloud");
  function("add_rgb", &api::add_rgb, "Add rgb attributes to the point cloud");
  function("aggregate", &api::aggregate, "Rasterize with R expression");
  function("callback", &api::callback, "Callback R function on the point cloud");
  function("classify_with_sor", &api::classify_with_sor, "Classify noise with SOR");
  function("classify_with_ivf", &api::classify_with_ivf, "Classify noise with IVF");
  function("classify_with_csf", &api::classify_with_csf, "Classify ground with CSF");
  function("delete_points", &api::delete_points, "Delete points that match criteria");
  function("edit_attribute", &api::edit_attribute, "Edit an attribute of the points");
  function("filter_with_grid", &api::filter_with_grid, "Filter points with a grid layout");
  function("focal", &api::focal, "Focal operation on a raster");
  function("geometry_feature", &api::geometry_features, "SVD decomposition and metrics");
  function("hull", &api::hull, "Compute hull of a point cloud");
  function("hull_triangulation", &api::hull_triangulation, "Compute hull of a triangulation");
  function("info", &api::info, "Filter points with a grid layout");
  function("load_raster", &api::load_raster, "Load a raster from file");
  function("load_matrix", &api::load_matrix, "Load a 4x4 matrix");
  function("local_maximum", &api::local_maximum, "Local maximum filter on a point cloud");
  function("local_maximum_raster", &api::local_maximum_raster, "Local maximum filter on a raster");
  function("neighborhood_metrics", &nonapi::neighborhood_metrics, "Local metrics");
  function("nothing", &nonapi::nothing, "A debugging stage");
  function("pit_fill", &api::pit_fill, "CHM enhancement");
  function("rasterize", &api::rasterize, "Rasterize point cloud");
  function("rasterize_triangulation", &api::rasterize_triangulation, "Rasterize a triangulation");
  function("reader_coverage", &api::reader_coverage, "Read points coverage");
  function("reader_circles", &api::reader_circles, "Read points within circles");
  function("reader_rectangles", &api::reader_rectangles, "Read points within rectangles");
  function("region_growing", &api::region_growing, "Region growing segmentation tree segmentation");
  function("remove_attribute", &api::remove_attribute, "Remove a named attribute from the point cloud");
  function("remove_attributes", &api::remove_attributes, "Remove named attributes from the point cloud");
  function("keep_attributes", &api::keep_attributes, "Keep named attributes from the point cloud");
  function("set_crs_epsg", &api::set_crs_epsg, "Set CRS using an EPSG code");
  function("set_crs_wkt", &api::set_crs_wkt, "Set CRS using a WKT string");
  function("sampling_voxel", &api::sampling_voxel, "Sample the point cloud using voxel-based sampling");
  function("sampling_pixel", &api::sampling_pixel, "Sample the point cloud using pixel-based sampling");
  function("sampling_poisson", &api::sampling_poisson, "Sample the point cloud using Poisson disk sampling");
  function("stop_if_outside", &api::stop_if_outside, "Stop processing if the chunk is outside given bounds");
  function("stop_if_chunk_id_below", &api::stop_if_chunk_id_below, "Stop processing if chunk ID is below threshold");
  function("sort_points", &api::sort_points, "Sort points spatially and/or in temporarly");
  function("summarise", &api::summarise, "Summarise metrics in vertical and intensity bins");
  function("triangulate", &api::triangulate, "Generate a triangulation from the point cloud");
  function("transform_with", &api::transform_with, "Transform the point cloud with");
  function("write_las", &api::write_las, "Write a LAS or LAZ file");
  function("write_copc", &api::write_copc, "Write a LAS or LAZ file");
  function("write_pcd", &api::write_pcd, "Write a PCD file");
  function("write_vpc", &api::write_vpc, "Write a VPC file");
  function("write_lax", &api::write_lax, "Write a LAX spatial index for LAS and LAZ files");
  function("xptr", &api::xptr, "Reserved for internal use only");
}

/* ==========================================
 *  FROM HERE, ONLY VERY R SPECIFIC TOOLS
 *  EXPOSED FOR THE R API
 *  =========================================
 */

inline api::Pipeline* as_pipeline(SEXP ptr)
{
  if (R_ExternalPtrAddr(ptr) == nullptr)
    throw std::invalid_argument("Invalid external pointer: nullptr");

  void* raw_ptr = R_ExternalPtrAddr(ptr);
  auto* p = dynamic_cast<api::Pipeline*>(static_cast<api::Pipeline*>(raw_ptr));

  if (p == nullptr)
    throw std::invalid_argument("Invalid external pointer: not a pointer on Pipeline");

  return p;
}

/*
 * Get some info about a stage such as the names, uid and class. This allow
 * to throw error at R level when connecting stages.
 */
Rcpp::List get_stage_info(SEXP ptr)
{
  api::Pipeline* p = as_pipeline(ptr);

  auto stages = p->get_stages();
  if (stages.size() != 1)
    throw std::invalid_argument(std::string("Invalid number of stages: input pipeline has ") + std::to_string(stages.size()) + " stages. Expected 1.");

  const api::Stage& s = *stages.begin();

  return Rcpp::List::create(
    _["name"] = s.get_name(),
    _["uid"] = s.get_uid(),
    _["raster"] = s.is_raster(),
    _["matrix"] = s.is_matrix(),
    _["vector"] = s.is_vector());
}

/*
 * R wrapper allowing to access a stage by index like a list
 * pipeline[[2]]. It could be used like that: transform_with(dtm[[2]])
 * where dtm is actually triangulate + rasterize
 */
SEXP get_stage_by_index(SEXP ptr, int i)
{
  api::Pipeline* p = as_pipeline(ptr);
  api::Pipeline res = (*p)[i];
  return wrap(res);
}

/*
 * R print
 */
SEXP print_pipeline(SEXP ptr)
{
  api::Pipeline* p = as_pipeline(ptr);

  std::string out = p->to_string();
  Rprintf("%s\n", out.c_str());
  return R_NilValue;
}

/*
 * R wrapper to generate the json file
 */
std::string generate_json(SEXP ptr, std::vector<std::string> on, Rcpp::List with)
{
  // Initialize with default values
  double buffer = 0.0;
  double chunk = 0.0;
  bool verbose = false;
  bool progress = true;
  std::string strategy = "concurrent-points";
  std::string profile_file = "";
  std::string progress_file = "";
  std::string log_file = "";
  std::vector<int> ncores = {1, 0};
  std::vector<bool> noprocess;

  // Helper lambda to update a variable if present in 'with'
  auto update_if_present = [&with](auto& var, const char* name) {
    if (with.containsElementNamed(name) && !Rf_isNull(with[name])) {
      var = Rcpp::as<std::decay_t<decltype(var)>>(with[name]);
    }
  };

  // Apply updates
  update_if_present(buffer, "buffer");
  update_if_present(chunk, "chunk");
  update_if_present(verbose, "verbose");
  update_if_present(progress, "progress");
  update_if_present(strategy, "strategy");
  update_if_present(profile_file, "profile_file");
  update_if_present(progress_file, "progress_file");
  update_if_present(log_file, "log_file");
  update_if_present(ncores, "ncores");
  update_if_present(noprocess, "noprocess");

  // Retrieve pipeline and make a deep copy
  api::Pipeline* tmp = as_pipeline(ptr);
  api::Pipeline p(*tmp);

  // Set parameters
  p.set_files(on);
  p.set_verbose(verbose);
  p.set_buffer(buffer);
  p.set_progress(progress);
  p.set_chunk(chunk);
  p.set_profile_file("");
  p.set_noprocess(noprocess);
  p.set_log_file(log_file);
  p.set_progress_file(progress_file);
  p.set_profile_file(profile_file);

  if (strategy == "sequential")
    p.set_sequential_strategy();
  else if (strategy == "concurrent-points")
    p.set_concurrent_points_strategy(ncores[0]);
  else if (strategy == "concurrent-files")
    p.set_concurrent_files_strategy(ncores[0]);
  else if(strategy == "nested")
    p.set_nested_strategy(ncores[0], ncores[1]);
  else
    throw std::invalid_argument("Invalid strategy");

  // Generate and execute the pipeline
  SEXP call = PROTECT(Rf_lang1(Rf_install("tempdir")));
  SEXP result = PROTECT(Rf_eval(call, R_GlobalEnv));
  const char* path_cstr = CHAR(STRING_ELT(result, 0));
  std::string path(path_cstr);
  UNPROTECT(2);
  path += "/pipeline.json";
  std::string f = p.write_json(path);
  return f;
}

/*
 * R wrapper to execute the json file
 */
SEXP execute_pipeline(std::string f)
{
  return api::execute(f);
}

/*
 * Get the address of a SEXP in order to pass the address of an xptr or a data.frame
 * via the json file
 */
SEXP get_address(SEXP obj)
{
  char address[20];
  snprintf(address, sizeof(address), "%p", (void*)obj);
  return Rf_mkString(address);
}

/*
 * R wrapper to concatenate pipelines. It allows to write
 * pipeline = stage1 + stage2 at R level
 */
SEXP merge_pipeline(SEXP e1, SEXP e2)
{
  api::Pipeline* p1 = as_pipeline(e1);
  api::Pipeline* p2 = as_pipeline(e2);
  api::Pipeline p = *p1 + *p2;
  return wrap(p);
}

/*
 * R wrapper to pipeline_info()
 */
SEXP pipeline_info(SEXP ptr)
{
  api::Pipeline* p = as_pipeline(ptr);
  std::string f = p->write_json();
  api::PipelineInfo info = api::pipeline_info(f);

  return List::create(
    _["streamable"]     = info.streamable,
    _["read_points"]    = info.read_points,
    _["buffer"]         = info.buffer,
    _["parallelizable"] = info.parallelizable,
    _["parallelized"]   = info.parallelized,
    _["R_API"]          = info.use_rcapi
  );
}

/*
 * Special function for the R API that allow to deal with LAS object from lidR
 * It custom edits the reader stage and add a build_catalog stage that enable to deal
 * with a data.frame in a completely non api way.
 */
SEXP cast_pipeline_to_dataframe_compatible(SEXP ptr, std::string addr, std::string crs, std::vector<double> accuracy)
{
  api::Pipeline* tmp = as_pipeline(ptr);

  // Deep copy;
  api::Pipeline p(*tmp);

  if (!p.has_reader())
  {
    api::Pipeline reader = api::reader_coverage();
    p = reader + p;
  }

  auto& stages = p.get_stages();

  for (auto& stage : stages)
  {
    if (stage.get_name() == "reader")
    {
      stage.set("dataframe", addr);
      stage.set("crs", crs);
      stage.set("accuracy", accuracy);
      break;
    }
  }

  api::Stage s("build_catalog");
  s.set("dataframe", addr);
  s.set("crs", crs);
  s.set("type", "dataframe");

  api::Pipeline out = api::Pipeline(s);
  out += p;
  return wrap(out);
}

/*
 * Special function for the R API that allow to deal with external pointers
 * and point clouds loaded in memory as external pointers. It custom edits the
 * reader stage and add a build_catalog stage that enable to deal with xptr
 * in a completely non api way.
 */
SEXP cast_pipeline_to_xptr_compatible(SEXP ptr, std::string addr)
{
  api::Pipeline* tmp = as_pipeline(ptr);
  api::Pipeline p(*tmp); // Deep copy;

  if (!p.has_reader())
  {
    api::Pipeline reader = api::reader_coverage();
    p = reader + p;
  }

  auto& stages = p.get_stages();

  for (auto& stage : stages)
  {
    if (stage.get_name() == "reader")
    {
      stage.set("externalptr", addr);
      break;
    }
  }

  api::Stage s("build_catalog");
  s.set("externalptr", addr);
  s.set("type", "externalptr");

  api::Pipeline out = api::Pipeline(s);
  out += p;
  return wrap(out);
}

/*
 * This is used to create non API stages at R level without being constrained
 * by the C++ API. We can basically build any stage including invalid stages
 * for debugging purpose.
 */
SEXP make_stage(std::string stage_name, Rcpp::List opts)
{
  api::Stage s(stage_name);
  Rcpp::CharacterVector names = opts.names();

  for (int i = 0; i < opts.size(); ++i)
  {
    std::string key = Rcpp::as<std::string>(names[i]);
    Rcpp::RObject obj = opts[i];

    if (Rcpp::is<int>(obj))
      s.set(key, Rcpp::as<int>(obj));
    else if (Rcpp::is<double>(obj))
      s.set(key, Rcpp::as<double>(obj));
    else if (Rcpp::is<bool>(obj))
      s.set(key, Rcpp::as<bool>(obj));
    else if (Rcpp::is<std::string>(obj))
      s.set(key, Rcpp::as<std::string>(obj));
    else if (Rcpp::is<Rcpp::IntegerVector>(obj))
      s.set(key, Rcpp::as<std::vector<int>>(obj));
    else if (Rcpp::is<Rcpp::NumericVector>(obj))
      s.set(key, Rcpp::as<std::vector<double>>(obj));
    else if (Rcpp::is<Rcpp::CharacterVector>(obj))
      s.set(key, Rcpp::as<std::vector<std::string>>(obj));
    else
      Rcpp::stop("Unsupported type for key: " + key);
  }

  api::Pipeline p(s);
  return wrap(p);
}


RCPP_MODULE(operations)
{
  function("generate_json", &generate_json, "Write JSON on disk");
  function("excecute_pipeline", &execute_pipeline, "Execute pipeline");
  function("merge_pipeline", &merge_pipeline, "Concatenate pipelines");
  function("print_pipeline", &print_pipeline, "Print pipelines");
  function("get_address", &get_address, "Get address of a SEXP");
  function("get_stage_info", &get_stage_info, "Get stage info");
  function("get_stage_by_index", &get_stage_by_index, "Operator [[]]");
  function("get_pipeline_info", &pipeline_info, "Get pipeline info");
  function("make_stage", &make_stage, "Make a custom stage");
  function("cast_pipeline_to_dataframe_compatible", &cast_pipeline_to_dataframe_compatible, "");
  function("cast_pipeline_to_xptr_compatible", &cast_pipeline_to_xptr_compatible, "");
}


/*
 * This is useless and exists only for testing if it compiles and works
 */
SEXP cpp_test1(std::vector<std::string> on)
{
  using namespace api;

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path();  // platform independent
  std::filesystem::path temp_file = temp_dir / "test.las";

  Pipeline p;
  p.set_files(on);
  p.set_concurrent_points_strategy(8);
  p.set_progress(false);

  p += delete_points({"Z < 1.37"});
  p += write_las(temp_file.string());

  std::string file = p.write_json();

  return execute(file);
}


/*
 * This is useless and exists only for testing if it compiles and works
 */
SEXP cpp_test2(std::vector<std::string> on)
{
  using namespace api;

  //std::string on = "/folder/of/laz/tiles/"

  // platform independent tmp files
  std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
  std::filesystem::path temp_dsm = temp_dir / "dsm.tif";
  std::filesystem::path temp_dtm = temp_dir / "dtm.tif";

  Pipeline tri = triangulate(0, {"Classification %in% 2 9"});
  Pipeline dtm = rasterize_triangulation(tri.get_stages().front().get_uid(), 1, temp_dtm.string());

  Pipeline p;
  p += classify_with_sor();
  p += delete_points({"Classification == 18"});
  p += rasterize(1, 1, {"max"}, {""}, temp_dsm.string());
  p += tri;
  p += dtm;

  p.set_files(on);
  p.set_concurrent_files_strategy(8);
  p.set_progress(false);

  std::string file = p.write_json();

  return execute(file);
}

RCPP_MODULE(tests)
{
  function("cpp_test1", &cpp_test1, "Test 1");
  function("cpp_test2", &cpp_test2, "Test 2");
}


#endif
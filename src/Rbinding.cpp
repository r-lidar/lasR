#ifdef USING_R

#include <RcppCommon.h>

#include "api.h"
#include "openmp.h"
#include "RAM.h"

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

SEXP process(SEXP, SEXP);

// [[Rcpp::export]]
SEXP cpp_process(SEXP args, SEXP async) { return process(args, async); } // old API to be removed

using namespace Rcpp;

// Generic tools that could be used by any API
RCPP_MODULE(utils)
{
  function("lasfilterusage", &api::lasfilterusage, "LASlib filter list");
  function("lastransformusage", &api::lastransformusage, "LASlib transform list");
  function("available_threads", &available_threads, "Number of usable cores");
  function("has_omp_support", &has_omp_support,  "Is lasr build with OpenMP support");
  function("is_indexed", &api::is_indexed, "The LAS files is spatially indexed");
  function("getAvailableRAM", &getAvailableRAM, "FreeRAM");
  function("getTotalRAM", &getTotalRAM, "Total RAM");
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

SEXP print_pipeline(SEXP ptr)
{
  api::Pipeline* p = as_pipeline(ptr);

  std::string out = p->to_string();
  Rprintf("%s\n", out.c_str());
  return R_NilValue;
}

SEXP excecute_pipeline(SEXP ptr, std::vector<std::string> on, double buffer, double chunk, int ncores, bool verbose, bool progress)
{
  api::Pipeline* tmp = as_pipeline(ptr);
  api::Pipeline p(*tmp); // Deep copy. We do not want to modify in place because the R side must stay untouched

  p.set_files(on);
  p.set_ncores(1);
  //p->set_strategy();
  p.set_verbose(verbose);
  p.set_buffer(buffer);
  p.set_progress(progress);
  p.set_chunk(chunk);
  p.set_profile_file("");

  std::string f = p.write_json();

  return api::execute(f);
}

SEXP get_address(SEXP obj)
{
  char address[20];
  snprintf(address, sizeof(address), "%p", (void*)obj);
  return Rf_mkString(address);
}

SEXP merge_pipeline(SEXP e1, SEXP e2)
{
  api::Pipeline* p1 = as_pipeline(e1);
  api::Pipeline* p2 = as_pipeline(e2);
  api::Pipeline p = *p1 + *p2;
  return wrap(p);
}

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

SEXP get_stage_by_index(SEXP ptr, int i)
{
  api::Pipeline* p = as_pipeline(ptr);
  api::Pipeline res = (*p)[i];
  return wrap(res);
}

bool has_reader(SEXP ptr)
{
  api::Pipeline* p = as_pipeline(ptr);
  return p->has_reader();
}

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

SEXP cast_pipeline_to_xptr_compatible(SEXP ptr, std::string addr)
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


void cast_reader_to_xptr_reader()
{

}


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


SEXP test(std::vector<std::string> on)
{
  using namespace api;

  std::filesystem::path temp_dir = std::filesystem::temp_directory_path();  // platform independent
  std::filesystem::path temp_file = temp_dir / "test.las";

  Pipeline p;
  p.set_files(on);
  p.set_ncores(8);
  p.set_progress(true);

  p += info();
  p += delete_points({"Z > 1.37"});
  p += write_las(temp_file);

  std::string file = p.write_json();

  return execute(file);
}

RCPP_MODULE(operations)
{
  function("excecute_pipeline", &excecute_pipeline, "Execute pipeline");
  function("merge_pipeline", &merge_pipeline, "Concatenate pipelines");
  function("print_pipeline", &print_pipeline, "Print pipelines");
  function("get_address", &get_address, "Get address of a SEXP");
  function("get_stage_info", &get_stage_info, "Get stage info");
  function("get_pipeline_info", &pipeline_info, "Get pipeline info");
  function("get_stage_by_index", &get_stage_by_index, "Get stage by index");
  function("make_stage", &make_stage, "Make a custom stage");
  function("cast_pipeline_to_dataframe_compatible", &cast_pipeline_to_dataframe_compatible, "");
  function("cast_pipeline_to_xptr_compatible", &cast_pipeline_to_xptr_compatible, "");
}

#endif
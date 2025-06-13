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
SEXP get_pipeline_info(SEXP);

// [[Rcpp::export]]
SEXP cpp_process(SEXP args, SEXP async) { return process(args, async); } // old API to be removed

// [[Rcpp::export]]
SEXP cpp_get_pipeline_info(SEXP pipeline) { return get_pipeline_info(pipeline); } // old API to be removed

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
  function("add_extrabytes", &api::add_attribute, "Add an attribute to the point cloud");
  function("add_rgb", &api::add_rgb, "Add rgb attributes to the point cloud");
  function("classify_with_sor", &api::classify_with_sor, "Classify noise with SOR");
  function("classify_with_ivf", &api::classify_with_ivf, "Classify noise with IVF");
  function("classify_with_csf", &api::classify_with_ivf, "Classify ground with CSF");
  function("delete_points", &api::delete_points, "Delete points that match criteria");
  function("edit_attribute", &api::edit_attribute, "Edit an attribute of the points");
  function("filter_with_grid", &api::filter_with_grid, "Filter points with a grid layout");
  function("focal", &api::focal, "Focal operation on a raster");
  function("hull", &api::hull, "Compute hull of a point cloud or a triangulation");
  function("info", &api::info, "Filter points with a grid layout");
  function("load_raster", &api::load_raster, "Load a raster from file");
  function("load_matrix", &api::load_matrix, "Load a 4x4 matrix");
  function("local_maximum", &api::local_maximum, "Local maximum filter");
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
}

SEXP print_pipeline(SEXP ptr)
{
  if (R_ExternalPtrAddr(ptr) == nullptr)
    Rf_error("Invalid external pointer: nullptr");

  void* raw_ptr = R_ExternalPtrAddr(ptr);
  auto* s = dynamic_cast<api::Pipeline*>(static_cast<api::Pipeline*>(raw_ptr));

  if (s == nullptr)
    Rf_error("Invalid external pointer: not a pointer on Pipeline");

  std::string out = s->to_string();
  Rprintf("%s\n", out.c_str());
  return R_NilValue;
}

SEXP excecute_pipeline(SEXP ptr, std::vector<std::string> on)
{
  if (R_ExternalPtrAddr(ptr) == nullptr)
    Rf_error("Invalid external pointer: nullptr");

  void* raw_ptr = R_ExternalPtrAddr(ptr);
  auto* s = dynamic_cast<api::Pipeline*>(static_cast<api::Pipeline*>(raw_ptr));

  if (s == nullptr)
    Rf_error("Invalid external pointer: not a pointer on Pipeline");

  s->set_files(on);
  std::string f = s->write_json();

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
  if (R_ExternalPtrAddr(e1) == nullptr)
    Rf_error("Invalid external pointer: nullptr");

  if (R_ExternalPtrAddr(e2) == nullptr)
    Rf_error("Invalid external pointer: nullptr");

  void* ptr1 = R_ExternalPtrAddr(e1);
  auto* s1 = dynamic_cast<api::Pipeline*>(static_cast<api::Pipeline*>(ptr1));
  if (s1 == nullptr) Rf_error("Invalid external pointer: not a pointer on Pipeline");

  void* ptr2 = R_ExternalPtrAddr(e2);
  auto* s2 = dynamic_cast<api::Pipeline*>(static_cast<api::Pipeline*>(ptr2));
  if (s2 == nullptr) Rf_error("Invalid external pointer: not a pointer on Pipeline");

  api::Pipeline p = *s1 + *s2;

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
}

#endif
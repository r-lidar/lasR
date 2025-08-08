#define USING_PYTHON 1
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>

// Include the C++ API
#include "LASRapi/api.h"
#include "LASRcore/RAM.h"
#include "LASRcore/openmp.h"
#include "LASRcore/error.h"

namespace py = pybind11;

py::dict get_stage_info(api::Pipeline pipeline) {
    auto stages = pipeline.get_stages();
    if (stages.size() != 1)
        throw std::invalid_argument("Invalid number of stages: input pipeline has " + std::to_string(stages.size()) + " stages. Expected 1.");

    const api::Stage& s = *stages.begin();

    py::dict info;
    info["name"] = s.get_name();
    info["uid"] = s.get_uid();
    info["raster"] = s.is_raster();
    info["matrix"] = s.is_matrix();
    info["vector"] = s.is_vector();
    return info;
}

std::string extract_uid(py::object connect_uid) {
    if (py::isinstance<api::Pipeline>(connect_uid)) {
        auto info = get_stage_info(connect_uid.cast<api::Pipeline>());
        return info["uid"].cast<std::string>();
    } else if (py::isinstance<py::str>(connect_uid)) {
        return connect_uid.cast<std::string>();
    } else {
        throw std::invalid_argument("connect_uid must be a Pipeline or a string uid");
    }
}

// Normalize Python input (path-like, directory, or iterable of path-likes)
// into a list of LAS/LAZ file paths (recursively when a directory is provided).
static std::vector<std::string> normalize_files(py::object files_input) {
    namespace fs = std::filesystem;

    auto add_if_pointcloud = [](const fs::path &p, std::vector<std::string> &out) {
        if (!fs::exists(p) || !fs::is_regular_file(p)) return;
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".las" || ext == ".laz") {
            out.push_back(p.string());
        }
    };

    auto expand_path = [&](const std::string &path_str, std::vector<std::string> &out) {
        fs::path p(path_str);
        if (!fs::exists(p)) return; // ignore non-existing
        if (fs::is_directory(p)) {
            for (auto const &entry : fs::recursive_directory_iterator(p)) {
                if (entry.is_regular_file()) add_if_pointcloud(entry.path(), out);
            }
        } else {
            add_if_pointcloud(p, out);
        }
    };

    std::vector<std::string> out;

    // Try single path-like using os.fspath
    try {
        py::module_ os = py::module_::import("os");
        py::object fspath = os.attr("fspath");
        std::string p = py::str(fspath(files_input));
        expand_path(p, out);
        return out;
    } catch (const py::error_already_set &) {
        // fall through
    }

    // Try iterable of path-likes
    if (py::isinstance<py::sequence>(files_input) && !py::isinstance<py::str>(files_input)) {
        py::module_ os = py::module_::import("os");
        py::object fspath = os.attr("fspath");
        py::sequence seq = files_input.cast<py::sequence>();
        for (py::handle h : seq) {
            try {
                std::string p = py::str(fspath(h));
                expand_path(p, out);
            } catch (const py::error_already_set &) {
                // skip non-path-like entries
            }
        }
        return out;
    }

    throw std::invalid_argument("files must be a path-like or an iterable of path-like objects");
}

// Helper function to create rich results from execution results
py::object create_result(bool success, const nlohmann::json& json_results, const std::string& json_config_path) {
    auto results = py::dict();
    results["success"] = success;
    results["json_config"] = json_config_path;
    
    if (success) {
        // Success case - return data without redundant message
        if (!json_results.empty()) {
            std::string json_str = json_results.dump();
            py::object json_module = py::module_::import("json");
            results["data"] = json_module.attr("loads")(json_str);
        } else {
            results["data"] = py::list();  // Empty list instead of None for consistency
        }
    } else {
        // Error case - include error message and no data
        results["message"] = last_error.empty() ? "Pipeline execution failed" : last_error;
        results["data"] = py::none();
    }
    
    return results;
}


PYBIND11_MODULE(pylasr, m) {
    m.doc() = "Python bindings for LASR library - LiDAR and point cloud processing"; 
    m.attr("__version__") = "0.17.0";

    // System information functions
    m.def("available_threads", &available_threads,
          "Get the number of available threads");

    m.def("has_omp_support", &has_omp_support,
          "Check if OpenMP support is available");

    m.def("get_available_ram", &getAvailableRAM,
          "Get the available RAM in bytes");

    m.def("get_total_ram", &getTotalRAM,
          "Get the total RAM in bytes");

    // LAS utility functions
    m.def("las_filter_usage", &api::lasfilterusage,
          "Display LASlib filter usage information");

    m.def("las_transform_usage", &api::lastransformusage,
          "Display LASlib transform usage information");

    m.def("is_indexed", &api::is_indexed,
          "Check if a LAS file is spatially indexed",
          py::arg("file"));

    // Pipeline info structure
    py::class_<api::PipelineInfo>(m, "PipelineInfo")
        .def_readwrite("streamable", &api::PipelineInfo::streamable)
        .def_readwrite("read_points", &api::PipelineInfo::read_points)
        .def_readwrite("buffer", &api::PipelineInfo::buffer)
        .def_readwrite("parallelizable", &api::PipelineInfo::parallelizable)
        .def_readwrite("parallelized", &api::PipelineInfo::parallelized)
        .def_readwrite("use_rcapi", &api::PipelineInfo::use_rcapi);

    // Core API functions
    m.def("execute", [](const std::string& config_file, const std::string& async_communication_file) -> py::object {
          // Execute the C++ pipeline without GIL for performance
          auto [success, json_results] = [&]() {
              py::gil_scoped_release release;
              return api::execute(config_file, async_communication_file);
          }();
          
          // Use helper to create rich results (GIL is now acquired for Python operations)
          return create_result(success, json_results, config_file);
      },
      "Execute a pipeline from a JSON configuration file (files must be embedded in JSON)",
      py::arg("config_file"),
      py::arg("async_communication_file") = "");

    m.def("execute", [](api::Pipeline& pipeline, py::object files_input, const std::string& async_communication_file) -> py::object {
            // Make a copy of the pipeline and set files (similar to R API approach)
            auto files = normalize_files(files_input);
            if (files.empty()) {
                throw std::invalid_argument("No .las/.laz files found in the provided input");
            }
            api::Pipeline p(pipeline);
            p.set_files(files);
            std::string json_file = p.write_json();
            
            // Execute once and convert results (avoid double execution)
            auto [success, json_results] = api::execute(json_file, async_communication_file);
            return create_result(success, json_results, json_file);
      },
            "Execute a pipeline with specified files (mimics R API: exec(pipeline, on=files))",
            py::arg("pipeline"),
            py::arg("files"),
            py::arg("async_communication_file") = "");

    m.def("execute", [](api::Pipeline& pipeline, py::object files_input, const std::string& async_communication_file) -> py::object {
            // Accept a path-like (file or directory) or an iterable of path-likes
            auto files = normalize_files(files_input);
            if (files.empty()) {
                throw std::invalid_argument("No .las/.laz files found in the provided input");
            }
            api::Pipeline p(pipeline);
            p.set_files(files);
            std::string json_file = p.write_json();
            auto [success, json_results] = api::execute(json_file, async_communication_file);
            return create_result(success, json_results, json_file);
      },
            "Execute a pipeline with files or a catalog path (accepts path-like or iterable); only .las/.laz files are used",
            py::arg("pipeline"),
            py::arg("files"),
            py::arg("async_communication_file") = "");

    m.def("pipeline_info", &api::pipeline_info,
          "Get pipeline information from a JSON configuration file",
          py::arg("config_file"));

    // Export Pipeline class from the C++ API
    py::class_<api::Pipeline>(m, "Pipeline")
        .def(py::init<>(), "Create an empty pipeline")
        .def(py::init<const api::Stage&>(), "Create a pipeline with a single stage", py::arg("stage"))
        .def("__add__", [](const api::Pipeline& self, const api::Stage& stage) {
            return self + stage;
        }, "Add a stage to the pipeline", py::arg("stage"))
        .def("__add__", [](const api::Pipeline& self, const api::Pipeline& other) {
            return self + other;
        }, "Concatenate two pipelines", py::arg("other"))
        .def("__iadd__", [](api::Pipeline& self, const api::Stage& stage) -> api::Pipeline& {
            return self += stage;
        }, "Add a stage to the pipeline in-place", py::arg("stage"))
        .def("__iadd__", [](api::Pipeline& self, const api::Pipeline& other) -> api::Pipeline& {
            return self += other;
        }, "Concatenate pipeline in-place", py::arg("other"))
        .def("__getitem__", [](const api::Pipeline& self, std::size_t index) {
            return self[index];
        }, "Get a sub-pipeline at index", py::arg("index"))
        .def("set_files", &api::Pipeline::set_files, "Set input files", py::arg("files"))
        .def("set_sequential_strategy", &api::Pipeline::set_sequential_strategy, "Set sequential processing strategy")
        .def("set_concurrent_points_strategy", &api::Pipeline::set_concurrent_points_strategy, "Set concurrent points processing strategy", py::arg("ncores"))
        .def("set_concurrent_files_strategy", &api::Pipeline::set_concurrent_files_strategy, "Set concurrent files processing strategy", py::arg("ncores"))
        .def("set_nested_strategy", &api::Pipeline::set_nested_strategy, "Set nested processing strategy", py::arg("ncores1"), py::arg("ncores2"))
        .def("set_verbose", &api::Pipeline::set_verbose, "Set verbose mode", py::arg("verbose"))
        .def("set_buffer", &api::Pipeline::set_buffer, "Set buffer size", py::arg("buffer"))
        .def("set_progress", &api::Pipeline::set_progress, "Set progress display", py::arg("progress"))
        .def("set_chunk", &api::Pipeline::set_chunk, "Set chunk size", py::arg("chunk"))
        .def("set_profile_file", &api::Pipeline::set_profile_file, "Set profiling output file", py::arg("path"))
        .def("set_noprocess", &api::Pipeline::set_noprocess, "Set no-process flags", py::arg("noprocess"))
        .def("has_reader", &api::Pipeline::has_reader, "Check if pipeline has a reader stage")
        .def("has_catalog", &api::Pipeline::has_catalog, "Check if pipeline has a catalog stage")
        .def("to_string", &api::Pipeline::to_string, "Get string representation")
        .def("write_json", &api::Pipeline::write_json, "Write pipeline to JSON file", py::arg("path") = "")
        .def("execute", [](api::Pipeline& self, const std::vector<std::string>& files) -> py::object {
            self.set_files(files);
            std::string json_file = self.write_json();

            // Execute once and convert results (avoid double execution)
            auto [success, json_results] = api::execute(json_file, "");
            return create_result(success, json_results, json_file);
        }, "Execute the pipeline on files and return results", py::arg("files"))
        .def("execute", [](api::Pipeline& self, py::object files_input, const std::string& async_communication_file) -> py::object {
            auto files = normalize_files(files_input);
            if (files.empty()) {
                throw std::invalid_argument("No .las/.laz files found in the provided input");
            }
            self.set_files(files);
            std::string json_file = self.write_json();
            auto [success, json_results] = api::execute(json_file, async_communication_file);
            return create_result(success, json_results, json_file);
        }, "Execute the pipeline on a directory (catalog) or iterable of path-like; only .las/.laz are used",
           py::arg("files"), py::arg("async_communication_file") = "");

    // ==== STAGE CREATION FUNCTIONS ====
    // Following the exact C++ API signatures

    // Basic operations
    m.def("add_attribute", &api::add_attribute,
          "Add an attribute to the point cloud",
          py::arg("data_type"), py::arg("name"), py::arg("description"),
          py::arg("scale") = 1.0, py::arg("offset") = 0.0);

    m.def("add_rgb", &api::add_rgb,
          "Add RGB attributes to the point cloud");

    m.def("info", [](py::object file = py::none()) -> py::object {
    auto stage = api::info();
    if (file.is_none()) {
        // Return stage for pipeline
        api::Pipeline pipeline(stage);
        return py::cast(pipeline);
    } else if (py::isinstance<py::str>(file)) {
        // If a string is provided, execute the pipeline for a single file
        std::string file_path = file.cast<std::string>();
        api::Pipeline pipeline(stage);
        pipeline.set_files({file_path});
        std::string json_file = pipeline.write_json();
        api::execute(json_file);
        return py::none();
    } else {
        throw std::invalid_argument("info() accepts only a string file path or nothing");
    }
      }, py::arg("file") = py::none(), "Get point cloud information or execute info pipeline on a file");

    // Classification functions
    m.def("classify_with_sor", &api::classify_with_sor,
          "Classify noise using Statistical Outlier Removal",
          py::arg("k") = 8, py::arg("m") = 6, py::arg("classification") = 18);

    m.def("classify_with_ivf", &api::classify_with_ivf,
          "Classify noise using Isolated Voxel Filter",
          py::arg("res") = 5.0, py::arg("n") = 6, py::arg("classification") = 18);

    m.def("classify_with_csf", &api::classify_with_csf,
          "Classify ground using Cloth Simulation Filter",
          py::arg("slope_smooth") = false, py::arg("class_threshold") = 0.5,
          py::arg("cloth_resolution") = 0.5, py::arg("rigidness") = 1,
          py::arg("iterations") = 500, py::arg("time_step") = 0.65,
          py::arg("classification") = 2, py::arg("filter") = std::vector<std::string>{""});

    // Geometric features
    m.def("geometry_features", &api::geometry_features,
          "Compute geometric features",
          py::arg("k"), py::arg("r"), py::arg("features") = "");

    // Point operations
    m.def("delete_points", &api::delete_points,
          "Delete points matching filter criteria",
          py::arg("filter") = std::vector<std::string>{""});

    m.def("edit_attribute", &api::edit_attribute,
          "Edit attribute values",
          py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("attribute") = "", py::arg("value") = 0.0);

    m.def("remove_attribute", &api::remove_attribute,
          "Remove an attribute from the point cloud",
          py::arg("name"));

    m.def("sort_points", &api::sort_points,
          "Sort points",
          py::arg("spatial") = true);

    // Filtering
    m.def("filter_with_grid", &api::filter_with_grid,
          "Filter points using grid-based approach",
          py::arg("res"), py::arg("operation") = "min", 
          py::arg("filter") = std::vector<std::string>{""});

    // Sampling
    m.def("sampling_voxel", &api::sampling_voxel,
          "Sample points using voxel-based approach",
          py::arg("res") = 2.0, py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("method") = "random", py::arg("shuffle_size") = std::numeric_limits<int>::max());

    m.def("sampling_pixel", &api::sampling_pixel,
          "Sample points using pixel-based approach",
          py::arg("res") = 2.0, py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("method") = "random", py::arg("use_attribute") = "Z",
          py::arg("shuffle_size") = std::numeric_limits<int>::max());

    m.def("sampling_poisson", &api::sampling_poisson,
          "Sample points using Poisson disk sampling",
          py::arg("distance") = 2.0, py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("shuffle_size") = std::numeric_limits<int>::max());

    // Rasterization
    m.def("rasterize", &api::rasterize,
          "Rasterize point cloud",
          py::arg("res"), py::arg("window"), py::arg("operators") = std::vector<std::string>{"max"},
          py::arg("filter") = std::vector<std::string>{""}, py::arg("ofile") = "",
          py::arg("default_value") = -99999.0);

    m.def("rasterize_triangulation", [](py::object connect_uid, double res, std::string ofile) {
        std::string uid = extract_uid(connect_uid);
        return api::rasterize_triangulation(uid, res, ofile);
    },
    "Rasterize triangulated surface",
    py::arg("connect_uid"), py::arg("res"), py::arg("ofile") = "");

    // Raster operations
    m.def("focal", [](py::object connect_uid, int size, std::string fun, std::string ofile) {
        std::string uid = extract_uid(connect_uid);
        return api::focal(uid, size, fun, ofile);
    },
    "Apply focal operation",
    py::arg("connect_uid"), py::arg("size"), py::arg("fun") = "mean", py::arg("ofile") = "");

    m.def("pit_fill", [](py::object connect_uid, int lap_size, double thr_lap, double thr_spk, int med_size, int dil_radius, std::string ofile) {
        std::string uid = extract_uid(connect_uid);
        return api::pit_fill(uid, lap_size, thr_lap, thr_spk, med_size, dil_radius, ofile);
    },
    "Fill pits in raster",
    py::arg("connect_uid"), py::arg("lap_size") = 3, py::arg("thr_lap") = 0.1,
    py::arg("thr_spk") = -0.1, py::arg("med_size") = 3, py::arg("dil_radius") = 0, py::arg("ofile") = "");

    // Loading data
    m.def("load_raster", &api::load_raster,
          "Load raster from file",
          py::arg("file"), py::arg("band") = 1);

    m.def("load_matrix", &api::load_matrix,
          "Load transformation matrix",
          py::arg("matrix"), py::arg("check") = true);

    // Readers
    m.def("reader_coverage", &api::reader_coverage,
          "Read points from coverage area",
          py::arg("filter") = std::vector<std::string>{""}, py::arg("select") = "*", py::arg("copc_depth") = -1);

    m.def("reader_circles", &api::reader_circles,
          "Read points from circular areas",
          py::arg("xc"), py::arg("yc"), py::arg("r"), 
          py::arg("filter") = std::vector<std::string>{""}, py::arg("select") = "*", py::arg("copc_depth") = -1);

    m.def("reader_rectangles", &api::reader_rectangles,
          "Read points from rectangular areas",
          py::arg("xmin"), py::arg("ymin"), py::arg("xmax"), py::arg("ymax"),
          py::arg("filter") = std::vector<std::string>{""}, py::arg("select") = "*", py::arg("copc_depth") = -1);

    // Local maxima
    m.def("local_maximum", &api::local_maximum,
          "Find local maxima in point cloud",
          py::arg("ws"), py::arg("min_height") = 2.0,
          py::arg("filter") = std::vector<std::string>{""}, py::arg("ofile") = "",
          py::arg("use_attribute") = "Z", py::arg("record_attributes") = false);

    m.def("local_maximum_raster", [](py::object connect_uid, int ws, double min_height, std::vector<std::string> filter, std::string ofile) {
        std::string uid = extract_uid(connect_uid);
        return api::local_maximum_raster(uid, ws, min_height, filter, ofile);
    },
    "Find local maxima in raster",
    py::arg("connect_uid"), py::arg("ws"), py::arg("min_height") = 2.0,
    py::arg("filter") = std::vector<std::string>{""}, py::arg("ofile") = "");

    // Triangulation and hulls
    m.def("triangulate", &api::triangulate,
          "Triangulate point cloud",
          py::arg("max_edge") = 0.0, py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("ofile") = "", py::arg("use_attribute") = "Z");

    m.def("hull", &api::hull,
          "Compute convex hull",
          py::arg("ofile") = "");

    m.def("hull_triangulation", [](py::object connect_uid, std::string ofile) {
        std::string uid = extract_uid(connect_uid);
        return api::hull_triangulation(uid, ofile);
    },
    "Compute hull from triangulation",
    py::arg("connect_uid"), py::arg("ofile") = "");

    // Region growing
    m.def("region_growing", [](py::object connect_uid_raster, py::object connect_uid_seeds, double th_tree, double th_seed, double th_cr, double max_cr, std::string ofile) {
        std::string uid_raster = extract_uid(connect_uid_raster);
        std::string uid_seeds = extract_uid(connect_uid_seeds);
        return api::region_growing(uid_raster, uid_seeds, th_tree, th_seed, th_cr, max_cr, ofile);
    },
    "Perform region growing segmentation",
    py::arg("connect_uid_raster"), py::arg("connect_uid_seeds"),
    py::arg("th_tree") = 2.0, py::arg("th_seed") = 0.45, py::arg("th_cr") = 0.55,
    py::arg("max_cr") = 20.0, py::arg("ofile") = "");

    // Transformations
    m.def("transform_with", [](py::object connect_uid, const std::string& operation, const std::string& store_in_attribute, bool bilinear) {
        std::string uid = extract_uid(connect_uid);
        // Проверка на тип стадии (triangulate/raster/matrix) как раньше
        if (py::isinstance<api::Pipeline>(connect_uid)) {
            auto info = get_stage_info(connect_uid.cast<api::Pipeline>());
            std::string name = info["name"].cast<std::string>();
            bool is_raster = info["raster"].cast<bool>();
            bool is_matrix = info["matrix"].cast<bool>();
            if (name != "triangulate" && !is_raster && !is_matrix)
                throw std::invalid_argument("The stage must be a triangulation or a raster stage or a matrix stage.");
        }
        return api::transform_with(uid, operation, store_in_attribute, bilinear);
    }, "Transform points using raster or matrix",
    py::arg("connect_uid"), py::arg("operation") = "-", py::arg("store_in_attribute") = "", py::arg("bilinear") = true);

    // CRS operations
      m.def("set_crs", [](py::object crs) -> api::Pipeline {
      api::Stage s("set_crs");
      if (py::isinstance<py::int_>(crs)) {
            s.set("epsg", crs.cast<int>());
            s.set("wkt", "");
      } else if (py::isinstance<py::str>(crs)) {
            s.set("epsg", 0);
            s.set("wkt", crs.cast<std::string>());
      } else {
            throw std::invalid_argument("crs must be either EPSG code (int) or WKT string");
      }
      return api::Pipeline(s);
      }, "Set CRS using EPSG code or WKT string", py::arg("crs"));

    // Stop conditions
    m.def("stop_if_outside", &api::stop_if_outside,
          "Stop processing if outside bounds",
          py::arg("xmin"), py::arg("ymin"), py::arg("xmax"), py::arg("ymax"));

    m.def("stop_if_chunk_id_below", &api::stop_if_chunk_id_below,
          "Stop processing if chunk ID is below threshold",
          py::arg("index"));

    // Summary statistics
    m.def("summarise", &api::summarise,
          "Compute summary statistics",
          py::arg("zwbin") = 2.0, py::arg("iwbin") = 50.0, 
          py::arg("metrics") = std::vector<std::string>{},
          py::arg("filter") = std::vector<std::string>{""});

    // Writers
    m.def("write_las", &api::write_las,
          "Write LAS/LAZ file",
          py::arg("ofile"), py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("keep_buffer") = false);

    m.def("write_copc", &api::write_copc,
          "Write COPC file",
          py::arg("ofile"), py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("keep_buffer") = false, py::arg("max_depth") = -1, py::arg("density") = "dense");

    m.def("write_pcd", &api::write_pcd,
          "Write PCD file",
          py::arg("ofile"), py::arg("binary") = true);

    m.def("write_vpc", &api::write_vpc,
          "Write VPC file",
          py::arg("ofile"), py::arg("absolute_path") = false, py::arg("use_gpstime") = false);

    m.def("write_lax", &api::write_lax,
          "Write LAX spatial index",
          py::arg("embedded") = false, py::arg("overwrite") = false);

    // Non-API functions (for testing/debugging)
    m.def("nothing", &nonapi::nothing,
          "Do nothing (for testing)",
          py::arg("read") = false, py::arg("stream") = false, py::arg("loop") = false);

    m.def("neighborhood_metrics", [](py::object connect_uid, std::vector<std::string> metrics, int k, double r, std::string ofile) {
        std::string uid = extract_uid(connect_uid);
        return nonapi::neighborhood_metrics(uid, metrics, k, r, ofile);
    },
    "Compute neighborhood metrics",
    py::arg("connect_uid"), py::arg("metrics"), py::arg("k") = 10, py::arg("r") = 0.0, py::arg("ofile") = "");

    // Convenience functions
    m.def("create_pipeline", []() -> api::Pipeline {
        return api::Pipeline();
    }, "Create a new empty pipeline");

    // Helper function for common DTM pipeline
    m.def("dtm_pipeline", [](double resolution, const std::string& ofile) -> api::Pipeline {
        return api::rasterize(resolution, resolution, {"min"}, {"Classification %in% 2 9"}, ofile);
    }, "Create a DTM pipeline", py::arg("resolution"), py::arg("ofile"));

    // Helper function for common CHM pipeline  
    m.def("chm_pipeline", [](double resolution, const std::string& ofile) -> api::Pipeline {
        return api::rasterize(resolution, resolution, {"max"}, {"Classification != 2"}, ofile);
    }, "Create a CHM pipeline", py::arg("resolution"), py::arg("ofile"));
    // Check for updates in interactive sessions only
    try {
        py::module_ sys = py::module_::import("sys");
        // Only proceed if stdout is a terminal
        if (py::hasattr(sys.attr("stdout"), "isatty") &&
            sys.attr("stdout").attr("isatty")().cast<bool>()) {
            py::module_ utils = py::module_::import("utils");
            if (py::hasattr(utils, "check_update")) {
                utils.attr("check_update")();
            }
        }
    } catch (const std::exception &e) {
        // Not critical, ignore any errors
    }
}
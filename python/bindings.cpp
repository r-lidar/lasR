#define USING_PYTHON 1
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <string>
#include <vector>
#include <filesystem>

// Include the C++ API
#include "LASRapi/api.h"
#include "LASRcore/RAM.h"
#include "LASRcore/openmp.h"
#include "LASRcore/error.h"

// Forward declaration for execute function
extern api::ReturnType execute(const std::string& config_file, const std::string& async_communication_file = "");

namespace py = pybind11;

PYBIND11_MODULE(pylasr, m) {
    m.doc() = "Python bindings for LASR library - LiDAR and point cloud processing"; 
    m.attr("__version__") = "0.3.17";

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

    // Execute function
    m.def("execute", &execute,
          py::call_guard<py::gil_scoped_release>(),
          "Execute a pipeline from a JSON configuration file",
          py::arg("config_file"),
          py::arg("async_communication_file") = "");

    // Export Stage class from the C++ API
    py::class_<api::Stage>(m, "Stage")
        .def(py::init<const std::string&>(), "Create a stage with algorithm name", py::arg("algoname"))
        .def("set", [](api::Stage& self, const std::string& key, py::object value) {
            // Handle different Python types and convert to Stage::Value
            if (py::isinstance<py::int_>(value)) {
                self.set(key, value.cast<int>());
            } else if (py::isinstance<py::float_>(value)) {
                self.set(key, value.cast<double>());
            } else if (py::isinstance<py::bool_>(value)) {
                self.set(key, value.cast<bool>());
            } else if (py::isinstance<py::str>(value)) {
                self.set(key, value.cast<std::string>());
            } else if (py::isinstance<py::list>(value)) {
                // Try to determine the list type
                py::list lst = value.cast<py::list>();
                if (lst.size() > 0) {
                    if (py::isinstance<py::int_>(lst[0])) {
                        self.set(key, value.cast<std::vector<int>>());
                    } else if (py::isinstance<py::float_>(lst[0])) {
                        self.set(key, value.cast<std::vector<double>>());
                    } else if (py::isinstance<py::str>(lst[0])) {
                        self.set(key, value.cast<std::vector<std::string>>());
                    }
                }
            } else {
                throw std::runtime_error("Unsupported value type for stage parameter");
            }
        }, "Set a parameter value", py::arg("key"), py::arg("value"))
        .def("has", &api::Stage::has, "Check if parameter exists", py::arg("key"))
        .def("get", [](const api::Stage& self, const std::string& key) -> py::object {
            if (!self.has(key)) {
                return py::none();
            }
            auto value = self.get(key);
            return std::visit([](auto&& arg) -> py::object {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, int>) {
                    return py::cast(arg);
                } else if constexpr (std::is_same_v<T, double>) {
                    return py::cast(arg);
                } else if constexpr (std::is_same_v<T, bool>) {
                    return py::cast(arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    return py::cast(arg);
                } else if constexpr (std::is_same_v<T, std::vector<int>>) {
                    return py::cast(arg);
                } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                    return py::cast(arg);
                } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                    return py::cast(arg);
                } else {
                    return py::none();
                }
            }, value);
        }, "Get a parameter value", py::arg("key"))
        .def("get_name", &api::Stage::get_name, "Get the stage algorithm name")
        .def("get_uid", &api::Stage::get_uid, "Get the stage unique identifier")
        .def("to_string", &api::Stage::to_string, "Get string representation")
        .def("set_raster", &api::Stage::set_raster, "Set stage as raster output")
        .def("set_matrix", &api::Stage::set_matrix, "Set stage as matrix output")
        .def("set_vector", &api::Stage::set_vector, "Set stage as vector output")
        .def("is_raster", &api::Stage::is_raster, "Check if stage outputs raster")
        .def("is_matrix", &api::Stage::is_matrix, "Check if stage outputs matrix")
        .def("is_vector", &api::Stage::is_vector, "Check if stage outputs vector");

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
        .def("set_files", &api::Pipeline::set_files, "Set input files", py::arg("files"))
        .def("set_ncores", &api::Pipeline::set_ncores, "Set number of cores", py::arg("n"))
        .def("set_verbose", &api::Pipeline::set_verbose, "Set verbose mode", py::arg("verbose"))
        .def("set_buffer", &api::Pipeline::set_buffer, "Set buffer size", py::arg("buffer"))
        .def("set_progress", &api::Pipeline::set_progress, "Set progress display", py::arg("progress"))
        .def("set_chunk", &api::Pipeline::set_chunk, "Set chunk size", py::arg("chunk"))
        .def("set_profile_file", &api::Pipeline::set_profile_file, "Set profiling output file", py::arg("path"))
        .def("to_string", &api::Pipeline::to_string, "Get string representation")
        .def("write_json", &api::Pipeline::write_json, "Write pipeline to JSON file", py::arg("path") = "")
        .def("execute", [](api::Pipeline& self, const std::vector<std::string>& files) -> bool {
            self.set_files(files);
            std::string json_file = self.write_json();
            return execute(json_file);
        }, py::call_guard<py::gil_scoped_release>(), "Execute the pipeline on files", py::arg("files"));

    // Stage creation functions - following the C++ API
    m.def("add_attribute", &api::add_attribute,
          "Add an attribute to the point cloud",
          py::arg("data_type"), py::arg("name"), py::arg("description"),
          py::arg("scale") = 1.0, py::arg("offset") = 0.0);

    m.def("add_rgb", &api::add_rgb,
          "Add RGB attributes to the point cloud");

    m.def("classify_with_sor", &api::classify_with_sor,
          "Classify noise using Statistical Outlier Removal",
          py::arg("k") = 8, py::arg("m") = 6, py::arg("classification") = 18);

    m.def("classify_with_ivf", &api::classify_with_ivf,
          "Classify noise using Isolated Voxel Filter",
          py::arg("res") = 5.0, py::arg("n") = 6, py::arg("classification") = 18);

    m.def("geometry_features", &api::geometry_features,
          "Compute geometric features",
          py::arg("k"), py::arg("r"), py::arg("features") = "");

    m.def("delete_points", &api::delete_points,
          "Delete points matching filter criteria",
          py::arg("filter") = std::vector<std::string>{""});

    m.def("edit_attribute", &api::edit_attribute,
          "Edit attribute values",
          py::arg("filter") = std::vector<std::string>{""}, 
          py::arg("attribute") = "", py::arg("value") = 0.0);

    m.def("filter_with_grid", &api::filter_with_grid,
          "Filter points using grid-based approach",
          py::arg("res"), py::arg("operation") = "min", 
          py::arg("filter") = std::vector<std::string>{""});

    m.def("focal", &api::focal,
          "Apply focal operation",
          py::arg("connect_uid"), py::arg("size"), py::arg("fun") = "mean", py::arg("ofile") = "");

    m.def("hull", &api::hull,
          "Compute convex hull",
          py::arg("connect_uid") = "", py::arg("ofile") = "");

    m.def("info", &api::info,
          "Get point cloud information");

    m.def("load_raster", &api::load_raster,
          "Load raster from file",
          py::arg("file"), py::arg("band") = 1);

    m.def("load_matrix", &api::load_matrix,
          "Load transformation matrix",
          py::arg("matrix"), py::arg("check") = true);

    m.def("local_maximum", &api::local_maximum,
          "Find local maxima",
          py::arg("ws"), py::arg("min_height") = 2.0,
          py::arg("filter") = std::vector<std::string>{""}, py::arg("ofile") = "",
          py::arg("use_attribute") = "Z", py::arg("record_attributes") = false);

    m.def("local_maximum_raster", &api::local_maximum_raster,
          "Find local maxima in raster",
          py::arg("connect_uid"), py::arg("ws"), py::arg("min_height") = 2.0,
          py::arg("filter") = std::vector<std::string>{""}, py::arg("ofile") = "");

    m.def("nothing", &api::nothing,
          "Do nothing (for testing)",
          py::arg("read") = false, py::arg("stream") = false, py::arg("loop") = false);

    m.def("pit_fill", &api::pit_fill,
          "Fill pits in raster",
          py::arg("connect_uid"), py::arg("lap_size") = 3, py::arg("thr_lap") = 0.1,
          py::arg("thr_spk") = -0.1, py::arg("med_size") = 3, py::arg("dil_radius") = 0,
          py::arg("ofile") = "");

    m.def("transform_with", &api::transform_with,
          "Transform points using raster or matrix",
          py::arg("connect_uid"), py::arg("operation") = "-", 
          py::arg("store_in_attribute") = "", py::arg("bilinear") = true);

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

    // Convenience function for testing (similar to the R API)
    m.def("test", [](const std::vector<std::string>& files) -> bool {
        using namespace api;
        
        std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
        std::filesystem::path temp_file = temp_dir / "test.las";
        
        Pipeline p;
        p.set_files(files);
        p.set_ncores(8);
        p.set_progress(true);
        
        p += info();
        p += delete_points({"Z > 1.37"});
        p += write_las(temp_file.string());
        
        std::string json_file = p.write_json();
        return execute(json_file);
    }, py::call_guard<py::gil_scoped_release>(), "Test function for pipeline execution", py::arg("files"));
}

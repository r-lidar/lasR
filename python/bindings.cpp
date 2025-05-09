#define USING_PYTHON 1
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include "LASRcore/RAM.h"
#include "LASRcore/openmp.h"
#include "LASRcore/error.h"
#include "LASRcore/PointCloud.h"
#include "LASRcore/pipeline.h"
#include "LASRcore/CRS.h"
#include "LASRcore/FileCollection.h"
#include "LASRcore/Header.h"
#include "LASRcore/PointFilter.h"

// Forward declarations
extern bool process(const std::string& config_file);
extern std::string get_pipeline_info(const std::string& config_file);

namespace py = pybind11;

PYBIND11_MODULE(pylasr, m) {
    m.doc() = "Python bindings for LASR library - LiDAR and point cloud processing"; 
    m.attr("__version__") = "0.3.17";

    // Basic processing functions
    m.def("process", &process,
          py::call_guard<py::gil_scoped_release>(),
          "Process a pipeline configuration file",
          py::arg("config_file"));
          
    m.def("get_pipeline_info", &get_pipeline_info,
          "Get information about a pipeline configuration",
          py::arg("config_file"));

    // System information functions
    m.def("available_threads", &available_threads,
          "Get the number of available threads");

    m.def("has_omp_support", &has_omp_support,
          "Check if OpenMP support is available");

    m.def("get_available_ram", &getAvailableRAM,
          "Get the available RAM in bytes");

    m.def("get_total_ram", &getTotalRAM,
          "Get the total RAM in bytes");

    // Export Pipeline class
    py::class_<Pipeline>(m, "Pipeline")
        .def(py::init<>())
        .def("need_buffer", &Pipeline::need_buffer)
        .def("clear", &Pipeline::clear);
} 

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

    // Basic processing functions
    m.def("process", &process,
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

    // Export CRS class
    py::class_<CRS>(m, "CRS")
        .def(py::init<>())
        .def(py::init<int, bool>(), py::arg("code"), py::arg("error") = false)
        .def(py::init<const std::string&, bool>(), py::arg("wkt"), py::arg("error") = false)
        .def("get_epsg", &CRS::get_epsg, "Get the EPSG code")
        .def("get_wkt", &CRS::get_wkt, "Get the WKT representation")
        .def("is_valid", &CRS::is_valid, "Check if CRS is valid")
        .def("is_meters", &CRS::is_meters, "Check if units are meters")
        .def("is_feet", &CRS::is_feet, "Check if units are feet");

    // Export FileCollection (catalog) class
    py::class_<FileCollection>(m, "FileCollection")
        .def(py::init<>())
        .def("read", &FileCollection::read, py::arg("files"), py::arg("progress") = false)
        .def("set_buffer", &FileCollection::set_buffer)
        .def("add_query", py::overload_cast<double, double, double, double>(&FileCollection::add_query), 
             "Add rectangular query", py::arg("xmin"), py::arg("ymin"), py::arg("xmax"), py::arg("ymax"))
        .def("add_query", py::overload_cast<double, double, double>(&FileCollection::add_query), 
             "Add circular query", py::arg("x_center"), py::arg("y_center"), py::arg("radius"))
        .def("set_chunk_size", &FileCollection::set_chunk_size)
        .def("get_number_chunks", &FileCollection::get_number_chunks)
        .def("get_number_files", &FileCollection::get_number_files)
        .def("get_crs", &FileCollection::get_crs)
        .def("set_crs", &FileCollection::set_crs);

    // Export Header class first (needed for PointCloud)
    py::class_<Header>(m, "Header")
        .def(py::init<>());

    // Export PointCloud class - Note: PointCloud requires a Header
    py::class_<PointCloud>(m, "PointCloud")
        .def(py::init<Header*>(), py::arg("header"));

    // Export Pipeline class
    py::class_<Pipeline>(m, "Pipeline")
        .def(py::init<>())
        .def("need_buffer", &Pipeline::need_buffer)
        .def("clear", &Pipeline::clear);
} 
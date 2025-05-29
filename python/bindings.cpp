#define USING_PYTHON 1
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <string>
#include <map>
#include <vector>
#include "LASRcore/RAM.h"
#include "LASRcore/openmp.h"
#include "LASRcore/error.h"
#include "LASRcore/PointCloud.h"
#include "LASRcore/pipeline.h"
#include "LASRcore/CRS.h"
#include "LASRcore/FileCollection.h"
#include "LASRcore/Header.h"
#include "LASRcore/PointFilter.h"
#include "lasr_stages.h"

// Forward declarations
extern bool process(const std::string& config_file);
extern std::string get_pipeline_info(const std::string& config_file);

namespace py = pybind11;

// Helper function to convert Python dict to JSON string
std::string dict_to_json(const py::dict& dict) {
    std::stringstream ss;
    ss << "{";
    bool first = true;
    for (const auto& item : dict) {
        if (!first) ss << ",";
        ss << "\"" << item.first.cast<std::string>() << "\":";
        if (py::isinstance<py::str>(item.second)) {
            ss << "\"" << item.second.cast<std::string>() << "\"";
        } else if (py::isinstance<py::int_>(item.second)) {
            ss << item.second.cast<int>();
        } else if (py::isinstance<py::float_>(item.second)) {
            ss << item.second.cast<double>();
        } else if (py::isinstance<py::bool_>(item.second)) {
            ss << (item.second.cast<bool>() ? "true" : "false");
        } else if (py::isinstance<py::list>(item.second)) {
            ss << "[";
            auto list = item.second.cast<py::list>();
            bool list_first = true;
            for (const auto& list_item : list) {
                if (!list_first) ss << ",";
                if (py::isinstance<py::str>(list_item)) {
                    ss << "\"" << list_item.cast<std::string>() << "\"";
                } else if (py::isinstance<py::int_>(list_item)) {
                    ss << list_item.cast<int>();
                } else if (py::isinstance<py::float_>(list_item)) {
                    ss << list_item.cast<double>();
                }
                list_first = false;
            }
            ss << "]";
        }
        first = false;
    }
    ss << "}";
    return ss.str();
}


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

    // Export write_lax function
    m.def("write_lax", &write_lax,
          "Create a write_lax stage for creating spatial index files",
          py::arg("embedded") = false,
          py::arg("overwrite") = false);

    // Export Pipeline class
    py::class_<Pipeline>(m, "Pipeline")
        .def(py::init<>())
        .def("need_buffer", &Pipeline::need_buffer)
        .def("clear", &Pipeline::clear)
        .def("parse", &Pipeline::parse)
        .def("is_streamable", &Pipeline::is_streamable)
        .def("need_points", &Pipeline::need_points)
        .def("is_parallelizable", &Pipeline::is_parallelizable)
        .def("is_parallelized", &Pipeline::is_parallelized)
        .def("use_rcapi", &Pipeline::use_rcapi);

    // Export LASRstage class
    py::class_<LASRstage>(m, "LASRstage")
        .def(py::init<const std::string&, const std::string&, const std::string&, bool, bool>(),
             py::arg("algoname"), py::arg("output") = "", py::arg("filter") = "",
             py::arg("raster") = false, py::arg("vector") = false)
        .def_readwrite("algoname", &LASRstage::algoname)
        .def_readwrite("output", &LASRstage::output)
        .def_readwrite("filter", &LASRstage::filter)
        .def_readwrite("raster", &LASRstage::raster)
        .def_readwrite("vector", &LASRstage::vector)
        .def_readwrite("uid", &LASRstage::uid)
        .def("add_args", [](LASRstage& self, const std::string& key, const py::object& value) {
            if (py::isinstance<py::str>(value)) {
                self.args[key] = value.cast<std::string>();
            } else if (py::isinstance<py::int_>(value)) {
                self.args[key] = value.cast<int>();
            } else if (py::isinstance<py::float_>(value)) {
                self.args[key] = value.cast<double>();
            } else if (py::isinstance<py::bool_>(value)) {
                self.args[key] = value.cast<bool>();
            } else if (py::isinstance<py::list>(value)) {
                auto list = value.cast<py::list>();
                std::vector<std::string> vec;
                for (const auto& item : list) {
                    if (py::isinstance<py::str>(item)) {
                        vec.push_back(item.cast<std::string>());
                    }
                }
                self.args[key] = vec;
            }
        })
        .def("get_args", &LASRstage::get_args)
        .def("to_dict", &LASRstage::to_dict);

    // Export LASRpipeline class
    py::class_<LASRpipeline>(m, "LASRpipeline")
        .def(py::init<>())
        .def(py::init<LASRstage*>(), py::arg("stage") = nullptr)
        .def("add_stage", &LASRpipeline::add_stage)
        .def("__add__", [](const LASRpipeline& self, const LASRpipeline& other) {
            LASRpipeline result;
            result.stages = self.stages;
            result.stages.insert(result.stages.end(), other.stages.begin(), other.stages.end());
            return result;
        })
        .def("to_json", [](const LASRpipeline& self) {
            std::map<std::string, std::map<std::string, std::string>> stages_dict;
            for (const auto& stage : self.stages) {
                stages_dict[stage.algoname] = stage.to_dict();
            }
            return dict_to_json(py::cast(stages_dict));
        })
        .def_readwrite("stages", &LASRpipeline::stages);
} 

# Python API Implementation Following C++ API Approach

## Overview

This document outlines the implementation of the Python API for LASR following the unified C++ API approach suggested in [GitHub issue #155](https://github.com/r-lidar/lasR/issues/155). The goal is to create a single, maintainable API layer that can be exposed to both R and Python.

## Problem Statement

The original approach involved creating separate APIs for R and Python that would eventually get out of sync as new features were added. Jean-Romain proposed creating a unified C++ API that both language bindings could use, ensuring consistency and reducing maintenance overhead.

## Solution Architecture

### C++ API Layer (`src/LASRapi/api.h`)

The C++ API provides:
- `api::Stage` class for individual processing stages
- `api::Pipeline` class for combining and executing stages  
- Factory functions for creating common stages (e.g., `api::add_attribute()`, `api::classify_with_sor()`)
- Conditional compilation support for different target languages via `USING_R`, `USING_PYTHON` macros

### Python Bindings (`python/bindings.cpp`)

The new Python bindings:
1. **Include the C++ API**: `#include "LASRapi/api.h"`
2. **Define Python compilation**: `#define USING_PYTHON 1`
3. **Expose C++ API classes**: Bind `api::Stage` and `api::Pipeline` directly
4. **Expose factory functions**: Bind all stage creation functions from the C++ API
5. **Handle type conversions**: Convert between Python types and C++ `std::variant` types
6. **Provide Python-friendly interface**: Support Python operators like `+=` and `+`

## Key Implementation Details

### Type Handling

The `api::Stage::Value` type is a `std::variant` that can hold:
- `int`, `double`, `bool`, `std::string`
- `std::vector<int>`, `std::vector<double>`, `std::vector<std::string>`

The Python bindings automatically detect Python types and convert them appropriately:

```cpp
.def("set", [](api::Stage& self, const std::string& key, py::object value) {
    if (py::isinstance<py::int_>(value)) {
        self.set(key, value.cast<int>());
    } else if (py::isinstance<py::float_>(value)) {
        self.set(key, value.cast<double>());
    }
    // ... handle other types
})
```

### Pipeline Operations

Python pipelines support the same operations as R pipelines:

```python
# Create pipeline
pipeline = pylasr.Pipeline()

# Add stages with += operator
pipeline += pylasr.info()
pipeline += pylasr.classify_with_sor(k=10)

# Combine pipelines with + operator  
full_pipeline = preprocessing_pipeline + output_pipeline

# Execute directly
success = pipeline.execute(files)
```

### Stage Creation

All stage creation functions from the C++ API are exposed:

```cpp
m.def("classify_with_sor", &api::classify_with_sor,
      "Classify noise using Statistical Outlier Removal",
      py::arg("k") = 8, py::arg("m") = 6, py::arg("classification") = 18);
```

This ensures that when new stages are added to the C++ API, they can be easily exposed to Python by adding a single line.

## Benefits of This Approach

### 1. Consistency
The Python API exactly mirrors the R API structure and behavior.

### 2. Maintainability  
- Only the C++ API needs to be updated when adding new features
- Python bindings require minimal changes (just adding function bindings)
- No duplicate logic between R and Python implementations

### 3. Stability
- The C++ API provides a stable interface layer
- Internal LASR core changes don't affect the language bindings
- JSON serialization is handled consistently

### 4. Feature Parity
- New stages are automatically available to both R and Python
- Processing options and pipeline behavior are identical
- Error handling is consistent

## Comparison with Previous Approach

### Old Approach (Direct Binding)
```cpp
// Directly bind core classes
py::class_<Pipeline>(m, "Pipeline")
    .def("need_buffer", &Pipeline::need_buffer)
    .def("clear", &Pipeline::clear);

// Forward declarations to wrapper functions
extern bool process(const std::string& config_file);
```

### New Approach (C++ API Binding)
```cpp
// Bind C++ API classes
py::class_<api::Pipeline>(m, "Pipeline")
    .def("set_ncores", &api::Pipeline::set_ncores)
    .def("execute", [](api::Pipeline& self, const std::vector<std::string>& files) {
        self.set_files(files);
        return execute(self.write_json());
    });

// Bind C++ API factory functions
m.def("classify_with_sor", &api::classify_with_sor);
```

## Usage Examples

### Basic Pipeline
```python
import pylasr

pipeline = pylasr.Pipeline()
pipeline.set_ncores(4)
pipeline += pylasr.info()
pipeline += pylasr.classify_with_sor(k=10)
pipeline += pylasr.write_las("output.las")

success = pipeline.execute(["input.las"])
```

### Manual Stage Creation
```python
stage = pylasr.Stage("classify_with_sor")
stage.set("k", 12)
stage.set("classification", 18)

pipeline = pylasr.Pipeline(stage)
```

### Pipeline Combination
```python
preprocess = pylasr.info() + pylasr.classify_with_sor()
output = pylasr.write_las("result.las")
full_pipeline = preprocess + output
```

## Future Development

### Adding New Stages
When a new stage is added to the C++ API:

1. **C++ API** (`src/LASRapi/api.h`): Add factory function
   ```cpp
   Pipeline new_stage(int param1, double param2 = 1.0);
   ```

2. **Python Bindings** (`python/bindings.cpp`): Add binding
   ```cpp
   m.def("new_stage", &api::new_stage,
         "Description of new stage",
         py::arg("param1"), py::arg("param2") = 1.0);
   ```

3. **R Bindings** (`src/Rbinding.cpp`): Add binding
   ```cpp
   function("new_stage", &api::new_stage, "Description");
   ```

The stage implementation, JSON serialization, and execution logic are all handled by the C++ API layer.

### Extending Functionality
- New pipeline options can be added to `api::Pipeline`
- New parameter types can be added to `api::Stage::Value`
- System information functions can be added to the API namespace

## Build Configuration

The Python bindings require:
- `#define USING_PYTHON 1` to enable Python-specific code paths
- pybind11 for Python binding generation
- C++17 compiler for `std::variant` support

The C++ API uses conditional compilation:
```cpp
#if defined(USING_R)
using ReturnType = SEXP;
#elif defined(USING_PYTHON)  
using ReturnType = bool;
#else
using ReturnType = bool;
#endif
```

## Conclusion

This implementation successfully addresses the concerns raised in GitHub issue #155 by:
- Creating a unified C++ API that both R and Python can use
- Ensuring consistency between language bindings
- Reducing maintenance overhead
- Providing a stable interface for future development

The approach is scalable and maintainable, making it easy to add new features that are automatically available to both R and Python users. 
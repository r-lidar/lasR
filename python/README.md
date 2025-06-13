# LASR Python API

This Python API for LASR follows the unified C++ API approach suggested in [GitHub issue #155](https://github.com/r-lidar/lasR/issues/155). This ensures consistency between the R and Python APIs and reduces maintenance overhead.

## Architecture

The Python bindings now use the C++ API layer (`src/LASRapi/api.h`) instead of directly binding to the core LASR classes. This provides several benefits:

1. **Consistency**: The Python API mirrors the R API exactly
2. **Maintainability**: Only one API needs to be updated when new features are added
3. **Stability**: The C++ API provides a stable interface that won't change frequently
4. **Feature Parity**: New stages added to the C++ API are automatically available in Python

## Key Classes

### `Pipeline`
The main class for building and executing processing pipelines.

```python
import pylasr

# Create a pipeline
pipeline = pylasr.Pipeline()

# Configure processing options
pipeline.set_ncores(4)
pipeline.set_verbose(True)
pipeline.set_buffer(50.0)  # 50m buffer

# Add stages
pipeline += pylasr.info()
pipeline += pylasr.classify_with_sor(k=10, m=8)
pipeline += pylasr.write_las("output.las")

# Execute on files
files = ["input1.las", "input2.las"]
success = pipeline.execute(files)
```

### `Stage`
Individual processing stages that can be combined into pipelines.

```python
# Create a stage manually
stage = pylasr.Stage("classify_with_sor")
stage.set("k", 12)
stage.set("m", 6)
stage.set("classification", 18)

# Add to pipeline
pipeline = pylasr.Pipeline(stage)
```

## Available Stages

The Python API exposes all stages available in the C++ API:

### Point Cloud Processing
- `info()` - Get point cloud information
- `delete_points(filter)` - Delete points matching criteria
- `edit_attribute(filter, attribute, value)` - Edit point attributes
- `add_attribute(data_type, name, description, scale, offset)` - Add new attributes
- `add_rgb()` - Add RGB attributes

### Classification
- `classify_with_sor(k, m, classification)` - Statistical Outlier Removal
- `classify_with_ivf(res, n, classification)` - Isolated Voxel Filter

### Spatial Operations
- `filter_with_grid(res, operation, filter)` - Grid-based filtering
- `local_maximum(ws, min_height, filter, ofile, use_attribute, record_attributes)` - Find local maxima
- `geometry_features(k, r, features)` - Compute geometric features

### Raster Operations
- `load_raster(file, band)` - Load raster from file
- `focal(connect_uid, size, fun, ofile)` - Apply focal operations
- `pit_fill(connect_uid, lap_size, thr_lap, thr_spk, med_size, dil_radius, ofile)` - Fill pits
- `transform_with(connect_uid, operation, store_in_attribute, bilinear)` - Transform using raster/matrix

### Output
- `write_las(ofile, filter, keep_buffer)` - Write LAS/LAZ files
- `write_copc(ofile, filter, keep_buffer, max_depth, density)` - Write COPC files
- `write_pcd(ofile, binary)` - Write PCD files
- `write_vpc(ofile, absolute_path, use_gpstime)` - Write VPC files
- `write_lax(embedded, overwrite)` - Write LAX spatial index

### Utilities
- `load_matrix(matrix, check)` - Load transformation matrix
- `hull(connect_uid, ofile)` - Compute convex hull
- `nothing(read, stream, loop)` - No-op stage for testing

## System Information

```python
import pylasr

# Get system information
print(f"Available threads: {pylasr.available_threads()}")
print(f"OpenMP support: {pylasr.has_omp_support()}")
print(f"Available RAM: {pylasr.get_available_ram() / (1024**3):.2f} GB")

# Display LAS utilities
pylasr.las_filter_usage()
pylasr.las_transform_usage()

# Check if file is indexed
indexed = pylasr.is_indexed("file.las")
```

## Pipeline Execution

There are two ways to execute pipelines:

### Method 1: Direct execution
```python
pipeline = pylasr.Pipeline()
pipeline += pylasr.info()
pipeline += pylasr.write_las("output.las")

files = ["input.las"]
success = pipeline.execute(files)
```

### Method 2: JSON-based execution
```python
pipeline = pylasr.Pipeline()
pipeline.set_files(["input.las"])
pipeline += pylasr.info()
pipeline += pylasr.write_las("output.las")

# Write to JSON and execute
json_file = pipeline.write_json("pipeline.json")
success = pylasr.execute(json_file)
```

## Pipeline Combination

Pipelines can be combined using the `+` operator:

```python
# Create preprocessing pipeline
preprocess = pylasr.Pipeline()
preprocess += pylasr.info()
preprocess += pylasr.classify_with_sor()

# Create output pipeline
output = pylasr.Pipeline()
output += pylasr.write_las("output.las")

# Combine
full_pipeline = preprocess + output
```

## Error Handling

The API uses exceptions for error handling:

```python
try:
    pipeline = pylasr.Pipeline()
    pipeline += pylasr.info()
    success = pipeline.execute(["nonexistent.las"])
except RuntimeError as e:
    print(f"Pipeline execution failed: {e}")
```

## Comparison with R API

The Python API closely mirrors the R API structure:

**R:**
```r
library(lasR)

pipeline <- info() + 
           classify_with_sor(k=10) + 
           write_las("output.las")

execute(pipeline, on = "input.las")
```

**Python:**
```python
import pylasr

pipeline = pylasr.info() + \
           pylasr.classify_with_sor(k=10) + \
           pylasr.write_las("output.las")

pipeline.execute(["input.las"])
```

## Building

The Python bindings require:
- pybind11
- C++17 compiler
- LASR C++ API compiled with `USING_PYTHON=1`

Build configuration should define `USING_PYTHON` to enable Python-specific code paths in the C++ API.

## Future Development

When new stages are added to the C++ API (`src/LASRapi/api.h`), they can be easily exposed to Python by adding the corresponding function binding in `bindings.cpp`. The stage implementation and JSON serialization are handled by the C++ API layer.

This approach ensures that the Python API stays in sync with the R API and reduces the maintenance burden significantly. 
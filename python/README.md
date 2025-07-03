# LASR Python Bindings

Python bindings for the LASR (pronounced "laser") library - a high-performance LiDAR point cloud processing library.

## Overview

The LASR Python bindings provide a clean, Pythonic interface to the powerful LASR C++ library for processing large-scale LiDAR point clouds. The API closely mirrors the R API, ensuring consistency across language bindings.

## Features

- **High Performance**: Direct bindings to optimized C++ code
- **Complete API Coverage**: All LASR stages and operations available
- **Pythonic Interface**: Natural Python syntax with operator overloading
- **Pipeline Processing**: Chain operations for efficient processing
- **Multi-threading Support**: Leverage multiple cores for processing
- **Memory Efficient**: Minimal memory overhead through C++ backend

## Installation

### Prerequisites

- Python 3.7+
- pybind11
- C++17 compatible compiler
- GDAL (>= 2.2.3)
- GEOS (>= 3.4.0)
- PROJ (>= 4.9.3)

### Build from Source

```bash
cd python
pip install -e .
```

or using setuptools directly:

```bash
cd python
python setup.py build_ext --inplace
```

## Quick Start

```python
import pylasr

# Create a simple processing pipeline
pipeline = (pylasr.info() + 
           pylasr.classify_with_sor(k=10) + 
           pylasr.delete_points(["Classification == 18"]) +
           pylasr.write_las("cleaned.las"))

# Set processing options
pipeline.set_concurrent_points_strategy(4)
pipeline.set_progress(True)

# Execute on files
files = ["input.las", "input2.las"]
success = pipeline.execute(files)
```

## API Structure

### Core Classes

#### `Stage`
Represents a single processing stage with configurable parameters.

```python
# Create a stage manually
stage = pylasr.Stage("classify_with_sor")
stage.set("k", 12)
stage.set("m", 8)

# Or use convenience functions
stage = pylasr.classify_with_sor(k=12, m=8)
```

#### `Pipeline`
Container for multiple stages with execution management.

```python
# Create empty pipeline
pipeline = pylasr.Pipeline()

# Add stages
pipeline += pylasr.info()
pipeline += pylasr.classify_with_sor()

# Set processing strategy
pipeline.set_concurrent_files_strategy(2)

# Execute
pipeline.execute(["file1.las", "file2.las"])
```

### Processing Strategies

LASR supports different parallelization strategies:

```python
# Sequential processing
pipeline.set_sequential_strategy()

# Concurrent points (single file, multiple cores)
pipeline.set_concurrent_points_strategy(ncores=4)

# Concurrent files (multiple files, single core each)
pipeline.set_concurrent_files_strategy(ncores=2)

# Nested strategy (multiple files, multiple cores each)
pipeline.set_nested_strategy(ncores1=2, ncores2=4)
```

## Available Stages

### Classification
- `classify_with_sor()` - Statistical Outlier Removal
- `classify_with_ivf()` - Isolated Voxel Filter  
- `classify_with_csf()` - Cloth Simulation Filter

### Point Operations
- `delete_points()` - Remove points by filter
- `edit_attribute()` - Modify attribute values
- `filter_with_grid()` - Grid-based filtering
- `sort_points()` - Sort points spatially

### Sampling
- `sampling_voxel()` - Voxel-based sampling
- `sampling_pixel()` - Pixel-based sampling
- `sampling_poisson()` - Poisson disk sampling

### Rasterization
- `rasterize()` - Convert points to raster
- `rasterize_triangulation()` - Rasterize triangulated surface

### Geometric Analysis
- `geometry_features()` - Compute geometric features
- `local_maximum()` - Find local maxima
- `triangulate()` - Triangulation
- `hull()` - Convex hull

### I/O Operations
- `write_las()` - Write LAS/LAZ files
- `write_copc()` - Write COPC files
- `write_pcd()` - Write PCD files
- `write_vpc()` - Write VPC catalogs
- `write_lax()` - Write spatial index

### Coordinate Systems
- `set_crs_epsg()` - Set CRS by EPSG code
- `set_crs_wkt()` - Set CRS by WKT string

### Data Loading
- `load_raster()` - Load raster data
- `load_matrix()` - Load transformation matrix

## Examples and Getting Started

The `examples/` directory contains several scripts to help you get started:

### 🚀 Running Examples

```bash
# Basic usage examples
python examples/basic_usage.py

# Complete feature demonstration  
python examples/complete_example.py

# Create pipeline JSON files
python examples/create_pipelines.py

# Inspect pipeline JSON files
python examples/lasr_cli.py info_pipeline.json

# Process files with pipelines
python examples/lasr_cli.py info_pipeline.json data.las
python examples/lasr_cli.py terrain_pipeline.json file1.las file2.las
```

### 📋 Example Scripts

#### `basic_usage.py`
Simple examples covering the most common use cases:
- System information
- Basic pipeline creation
- Configuration options
- JSON export
- Data processing (if example data available)

#### `complete_example.py`
Comprehensive demonstration of all major features:
- Advanced pipeline workflows
- Processing configuration
- Convenience functions
- Manual stage creation
- Error handling

#### `create_pipelines.py`
Creates various pipeline JSON files for different workflows:
- Info pipeline
- Cleaning pipeline (noise removal)
- Terrain analysis (DTM/CHM)
- Point sampling
- Classification workflows

#### `lasr_cli.py`
Enhanced command-line tool for pipeline inspection and processing:
- Display pipeline information
- Execute pipelines with input files
- Show processing timing and results
- Support for multithreading configuration

### 🔧 Basic DTM/CHM Generation

```python
import pylasr

# Use convenience functions for common workflows
dtm_pipeline = pylasr.dtm_pipeline(1.0, "dtm.tif")
chm_pipeline = pylasr.chm_pipeline(0.5, "chm.tif")

# Combine workflows
terrain_analysis = dtm_pipeline + chm_pipeline
terrain_analysis.set_concurrent_files_strategy(2)

# Process data
success = terrain_analysis.execute(["forest.las"])
```

### Advanced Processing Workflow

```python
# Multi-stage processing
pipeline = pylasr.Pipeline()

# Noise removal and ground classification
pipeline += pylasr.classify_with_sor(k=8, m=6)
pipeline += pylasr.classify_with_csf()
pipeline += pylasr.delete_points(["Classification == 18"])

# Height normalization
dtm = pylasr.rasterize(1.0, 1.0, ["min"], ["Classification == 2"])
pipeline += dtm
pipeline += pylasr.transform_with(dtm.get_uid(), "-", "HeightAboveGround")

# Tree detection
chm = pylasr.rasterize(0.5, 0.5, ["max"], ["Classification != 2"])
pipeline += chm
pipeline += pylasr.local_maximum_raster(chm.get_uid(), 3.0, 2.0, 
                                       ofile="trees.shp")

# Final output
pipeline += pylasr.write_las("processed.las")

# Execute with nested parallelism
pipeline.set_nested_strategy(2, 4)
pipeline.execute(input_files)
```

### Format Conversion

```python
# Convert between formats
converter = (pylasr.write_las("output.laz") +
            pylasr.write_pcd("output.pcd") +
            pylasr.write_copc("output.copc.laz") +
            pylasr.write_lax())

converter.execute(["input.las"])
```

## System Information

```python
import pylasr

# Check system capabilities
print(f"Available threads: {pylasr.available_threads()}")
print(f"OpenMP support: {pylasr.has_omp_support()}")
print(f"Available RAM: {pylasr.get_available_ram() / (1024**3):.1f} GB")

# Check if file is indexed
indexed = pylasr.is_indexed("file.las")
```

## Pipeline Introspection

```python
# Save pipeline as JSON
pipeline = pylasr.classify_with_sor() + pylasr.write_las("out.las")
json_file = pipeline.write_json("my_pipeline.json")

# Get pipeline information
info = pylasr.pipeline_info(json_file)
print(f"Streamable: {info.streamable}")
print(f"Buffer needed: {info.buffer}")
print(f"Parallelizable: {info.parallelizable}")
```

## Error Handling

```python
try:
    success = pipeline.execute(files)
    if not success:
        print("Pipeline execution failed")
except Exception as e:
    print(f"Error: {e}")
```

## Performance Tips

1. **Use appropriate parallelization strategy** based on your data and hardware
2. **Set buffer size** for algorithms that need neighborhood information
3. **Chain operations** in pipelines to minimize I/O
4. **Use spatial indexing** with `write_lax()` for faster access
5. **Sort points** spatially with `sort_points()` for better cache performance

## Comparison with R API

The Python API closely mirrors the R API structure:

| R | Python |
|---|--------|
| `exec(pipeline, on = files)` | `pipeline.execute(files)` |
| `pipeline + stage` | `pipeline + stage` |
| `with = list(ncores = 4)` | `pipeline.set_concurrent_points_strategy(4)` |
| `filter = "Z > 10"` | `filter = ["Z > 10"]` |

## Contributing

See the main LASR repository for contribution guidelines.

## License

GPL-3 - see LICENSE file for details.

## Links

- [Main LASR Repository](https://github.com/r-lidar/lasR)
- [LASR Documentation](https://r-lidar.github.io/lasR/)
- [Issue Tracker](https://github.com/r-lidar/lasR/issues)
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
- **Rich Results**: Access detailed stage outputs and processing statistics
- **Structured Error Handling**: Comprehensive error information and debugging
- **Flexible Input Paths**: Pass a directory/catalog or an iterable of path-like objects; only .las/.laz files are used

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

# Execute pipeline (returns rich results)
files = ["input.las", "input2.las"]
result = pipeline.execute(files)
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

# Execute pipeline  
result = pipeline.execute(["file1.las", "file2.las"])
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
# Basic usage examples (no data processing)
python examples/basic_usage.py

# Basic usage with your data
python examples/basic_usage.py your_file.las

# Complete feature demonstration
python examples/complete_example.py

# Complete workflow with your data
python examples/complete_example.py your_file.las

# Create and use pipelines
python examples/create_pipelines.py

# Use command-line tool with pipeline files
python examples/lasr_cli.py your_data.las
python examples/lasr_cli.py file1.las file2.las
```

### 📋 Example Scripts

#### `basic_usage.py`
Simple examples covering the most common use cases:
- System information
- Basic pipeline creation
- Configuration options
- Direct pipeline execution
- Data processing (provide your own .las file as argument)

#### `complete_example.py`
Comprehensive demonstration of all major features:
- Advanced pipeline workflows
- Processing configuration
- Convenience functions
- Manual stage creation
- Multithreading comparison
- Error handling (provide your own .las file as argument)

#### `create_pipelines.py`
Demonstrates various pipeline creation and execution workflows:
- Info pipeline
- Cleaning pipeline (noise removal)
- Terrain analysis (DTM/DSM)
- Point sampling
- Classification workflows

#### `lasr_cli.py`
Enhanced command-line tool for data processing:
- Execute common pipelines on input files
- Show processing timing and results
- Support for multithreading configuration

### 🔧 Basic DTM/DSM Generation

```python
import pylasr

# Use convenience functions for common workflows
dtm_pipeline = pylasr.dtm(1.0, "dtm.tif")
dsm_pipeline = pylasr.dsm(0.5, "dsm.tif")

# Combine them
terrain_analysis = dtm_pipeline + dsm_pipeline
terrain_analysis.set_concurrent_files_strategy(2)

# Process data
result = terrain_analysis.execute(["forest.las"])
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
input_files = ["file1.las", "file2.las"]
result = pipeline.execute(input_files)
```

### Format Conversion

```python
# Convert between formats
converter = (pylasr.write_las("output.laz") +
            pylasr.write_pcd("output.pcd") +
            pylasr.write_copc("output.copc.laz") +
            pylasr.write_lax())

result = converter.execute(["input.las"])
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
# Create pipeline and inspect properties directly
pipeline = pylasr.classify_with_sor() + pylasr.write_las("out.las")

# Check pipeline properties
print(f"Has reader: {pipeline.has_reader()}")
print(f"Has catalog: {pipeline.has_catalog()}")
print(f"Pipeline string: {pipeline.to_string()}")

```

## Results and Error Handling

### Result Format

Pipeline execution now returns detailed structured results:

```python
try:
    result = pipeline.execute(files)
    
    if result['success']:
        print("✅ Pipeline executed successfully!")
        print(f"Config saved to: {result['json_config']}")
        
        # Access stage results
        if result['data']:
            for stage_result in result['data']:
                for stage_name, stage_data in stage_result.items():
                    print(f"Stage '{stage_name}': {type(stage_data)}")
                    
                    # Example: Summary statistics
                    if stage_name == 'summary':
                        print(f"  Points: {stage_data['npoints']}")
                        print(f"  CRS: EPSG:{stage_data['crs']['epsg']}")
                    
                    # Example: File outputs
                    elif isinstance(stage_data, list):
                        print(f"  Files created: {stage_data}")
    else:
        print(f"❌ Pipeline failed: {result.get('message', 'Unknown error')}")
        
except Exception as e:
    print(f"❌ Exception during execution: {e}")
```

### Result Structure

**Successful Execution:**
```python
{
    "success": True,
    "json_config": "/path/to/config.json",
    "data": [
        {"summary": {"npoints": 12345, "crs": {...}, ...}},
        {"rasterize": ["/path/to/output.tif"]},
        {"write_las": ["/path/to/output.las"]}
    ]
}
```

**Failed Execution:**
```python
{
    "success": False, 
    "message": "Error description",
    "json_config": "/path/to/config.json",
    "data": None
}
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
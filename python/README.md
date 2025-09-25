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

### Input/Reading
- `reader()` - Basic point cloud reader
- `reader_coverage()` - Read points from coverage area
- `reader_circles()` - Read points within circular areas
- `reader_rectangles()` - Read points within rectangular areas

### Classification & Filtering
- `classify_with_sor()` - Statistical Outlier Removal classification
- `classify_with_ivf()` - Isolated Voxel Filter classification  
- `classify_with_csf()` - Cloth Simulation Filter (ground classification)
- `delete_points()` - Remove points by filter criteria
- `delete_noise()` - Remove noise points (convenience function)
- `delete_ground()` - Remove ground points (convenience function)
- `filter_with_grid()` - Grid-based point filtering

### Point Operations & Attributes
- `edit_attribute()` - Modify point attribute values
- `add_extrabytes()` - Add custom attributes to points
- `add_rgb()` - Add RGB color information
- `remove_attribute()` - Remove attributes from points
- `sort_points()` - Sort points spatially for better performance
- `transform_with()` - Transform points using raster operations

### Sampling & Decimation
- `sampling_voxel()` - Voxel-based point sampling
- `sampling_pixel()` - Pixel-based point sampling
- `sampling_poisson()` - Poisson disk sampling for uniform distribution

### Rasterization & Gridding
- `rasterize()` - Convert points to raster grids (DTM, DSM, CHM, etc.)
- `rasterize_triangulation()` - Rasterize triangulated surfaces
- `focal()` - Apply focal operations on rasters
- `pit_fill()` - Fill pits in canopy height models

### Geometric Analysis & Features
- `geometry_features()` - Compute geometric features (eigenvalues, etc.)
- `local_maximum()` - Find local maxima in point clouds
- `local_maximum_raster()` - Find local maxima in rasters (tree detection)
- `triangulate()` - Delaunay triangulation of points  
- `hulls()` - Compute convex hulls

### Segmentation & Tree Detection
- `region_growing()` - Region growing segmentation for tree detection

### Data Loading & Transformation
- `load_raster()` - Load external raster data
- `load_matrix()` - Load transformation matrices

### I/O Operations
- `write_las()` - Write LAS/LAZ files
- `write_copc()` - Write Cloud Optimized Point Cloud files
- `write_pcd()` - Write Point Cloud Data files
- `write_vpc()` - Write Virtual Point Cloud catalogs
- `write_lax()` - Write spatial index files

### Coordinate Systems
- `set_crs()` - Set coordinate reference system

### Information & Analysis  
- `info()` - Get point cloud information and statistics
- `summarise()` - Generate summary statistics

### Utility & Development
- `callback()` - Custom callback functions for advanced processing
- `nothing()` - No-operation stage for debugging
- `stop_if_outside()` - Stop processing if outside bounds
- `stop_if_chunk_id_below()` - Conditional processing based on chunk ID

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
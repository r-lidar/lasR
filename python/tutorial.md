 LASR Python Tutorial

**Author:** Adapted for Python from R version by Jean-Romain Roussel  
**Language:** Python

> **Note:** This tutorial has been adapted from the R API (`lasR` package) to Python (`pylasr` bindings). The concepts are identical, but the syntax follows Python conventions. This tutorial focuses on general concepts and practical implementation.

This tutorial is intended to be read in order. It introduces the available tools of the package in a specific order so that the reader can discover all the features of the package organically.

> **Important:** In the following tutorial, the variable `files` refers to one or several file paths stored in a list. It can also be the path to a directory. In this tutorial, the output is rendered with one or two small LAS files, but every pipeline is designed to support the processing of numerous files covering a large extent.

## Overall functionality

In `pylasr`, the Python functions provided to the user are not designed to process the data directly; instead, they are used to create a pipeline. A pipeline consists of atomic stages that are applied to a point cloud in order. Each stage can either transform the point cloud within the pipeline without generating any output or process the point cloud to produce an output.

In the conceptual diagram, there are 4 LAS/LAZ files and a pipeline that:
1. reads a file
2. builds and writes a DTM on disk  
3. transforms the point cloud by normalizing the elevation
4. builds a canopy height model using the transformed point cloud
5. transforms the point cloud by removing points below 5 m

The resulting version of the point cloud (points above 5m) is discarded and lost because there is no additional stage in this pipeline. However, other stages can be added, such as the application of a predictive model for points above 5 m or a stage that writes the point cloud to disk.

Once the first file completes the entire pipeline, the second file is used, and the pipeline is applied to fill in the missing parts of the geospatial rasters or vectors produced by the pipeline. **Each file is loaded with a buffer from neighboring files if needed**.

A pipeline created from the Python interface does nothing initially. After building the pipeline, users must call the `execute()` method on it to initiate the computation.

## Setup and imports

```python
import pylasr
import tempfile
import os

# System information
print(f"pylasr version: {pylasr.__version__}")
print(f"Available threads: {pylasr.available_threads()}")
print(f"OpenMP support: {pylasr.has_omp_support()}")
print(f"Available RAM: {pylasr.get_available_ram()} MB")
print(f"Total RAM: {pylasr.get_total_ram()} MB")

# Helper function for temporary files
def temp_file(suffix=".tif"):
    """Creates a temporary file"""
    return tempfile.NamedTemporaryFile(delete=False, suffix=suffix).name

def temp_gpkg():
    """Creates a temporary geopackage file"""
    return temp_file(".gpkg")

# Example files (replace with real paths)
f = "path/to/your/file.las"  # Single file
files = ["file1.las", "file2.las"]  # Multiple files
```

## Reader

The `reader_coverage()` stage MUST be the first stage of any pipeline. This stage reads the point cloud. When creating a pipeline with only this stage, the headers of the files are read, but no computation is actually applied. No result is returned.

```python
# Create a simple reading pipeline
pipeline = pylasr.reader_coverage()
result = pipeline.execute(f)
print(result)  # {'success': True, 'data': []}
```

In practice, when using `reader_coverage()` without arguments, it can be omitted - the `execute` function adds it on-the-fly.

## Triangulate

The first stage we can try is `triangulate()`. This algorithm performs a Delaunay triangulation on the points of interest. Triangulating points is a very useful task that is employed in numerous processing tasks. Triangulating all points is not very interesting, so we usually want to use the `filter` argument to triangulate only specific points of interest.

In the following example, we triangulate the points classified as 2 (i.e., ground). This produces a meshed Digital Terrain Model. The files are read sequentially, with points loaded one by one and stored to build a Delaunay triangulation. The program stores the point cloud and the Delaunay triangulation for the current processing file. Then the data are discarded to load a new file.

If the users do not provide a path to an output file to store the result, the result is lost. In the following pipeline, we are building a triangulation of the ground points, but we get no output because everything is lost.

```python
pipeline = pylasr.reader_coverage() + pylasr.triangulate(filter=["Classification == 2"])
result = pipeline.execute(f)
print(result)  # {'success': True, 'data': []}
```

In the following pipeline, the triangulation is stored in a geopackage file by providing an `ofile` argument:

```python
pipeline = pylasr.reader_coverage() + pylasr.triangulate(
    filter=["Classification == 2"], 
    ofile=temp_gpkg()
)
result = pipeline.execute(f)
print(f"Triangulation result: {result['success']}")
```

We can also triangulate the first returns. This produces a meshed Digital Surface Model:

```python
pipeline = pylasr.triangulate(filter=["ReturnNumber == 1"], ofile=temp_gpkg())
result = pipeline.execute(f)
```

We can also perform both triangulations in the same pipeline. **The idea of `pylasr` is to execute all the tasks in one pass using a pipeline:**

```python
del1 = pylasr.triangulate(filter=["Classification == 2"], ofile=temp_gpkg())
del2 = pylasr.triangulate(filter=["ReturnNumber == 1"], ofile=temp_gpkg())
pipeline = del1 + del2
result = pipeline.execute(f)
```

Using `triangulate()` without any other stage in the pipeline is usually not very useful. Typically, `triangulate()` is employed without the `ofile` argument as an intermediate step. For instance, it can be used with `rasterize_triangulation()`.

## Rasterize

`rasterize()` does exactly what users may expect from it and even more. There are two main variations:

1. Rasterize a Delaunay triangulation using `rasterize_triangulation()`
2. Rasterize with predefined operators using `rasterize()`. The operators are optimized internally, making the operations as fast as possible.

With these variations, users can build a CHM, a DTM, a predictive model, or anything else.

### Rasterize - triangulation

Let's build a DTM using a triangulation of the ground points. In the following pipeline, the LAS files are read, points are loaded for each LAS file **with a buffer**, a Delaunay triangulation of the ground points is built, and then the triangulation is interpolated and rasterized. By default, `rasterize_triangulation()` writes the raster in a temporary file, so the result is not discarded.

```python
# Build DTM from ground points triangulation
triangulation = pylasr.triangulate(filter=["Classification == 2"])
dtm = pylasr.rasterize_triangulation(triangulation, res=1.0)
pipeline = triangulation + dtm
result = pipeline.execute(f)
print(f"DTM created: {result['success']}")
```

Here, `execute()` returns the raster data because `triangulate()` without `ofile` returns nothing (None). Therefore, the pipeline contains two stages, but only one returns something.

Notice that, contrary to higher-level packages, there is usually no high-level function with names like `rasterize_terrain()`. Instead, `pylasr` is made up of low-level atomic stages that are more versatile but also more challenging to use. Two small convenience pipelines (`dtm()` and `dsm()`) are provided; they simply wrap the patterns illustrated here and are described later.

### Rasterize - internal metrics

Internal metrics are strings with a format `attribute_function`. `attribute` is an attribute of the point cloud such as `z`, `classification`, or `intensity`. `function` is an available metrics function such as `mean`, `max`, or `sd`. The following are examples of valid metric strings: `z_max`, `i_mean`, `intensity_mean`, `classification_mode`, `z_sd`.

Let's build two CHMs: one based on the highest point per pixel with a resolution of 2 meters, and the second based on the triangulation of the first returns with a resolution of 50 cm.

```python
# CHM using highest points per pixel (2m resolution)
chm1 = pylasr.rasterize(res=2.0, window=2.0, operators=["max"])

# CHM from triangulation of first returns (0.5m resolution)  
triangulation = pylasr.triangulate(filter=["ReturnNumber == 1"])
chm2 = pylasr.rasterize_triangulation(triangulation, res=0.5)

pipeline = triangulation + chm1 + chm2
result = pipeline.execute(f)
```

In the pipeline above, we are using two variations of rasterization: one capable of rasterizing the point cloud with a predefined operator (here `max` is interpreted as `z_max`), and the other capable of rasterizing a triangulation. The output contains both rasters.

## Transform with

Another way to use a Delaunay triangulation is to transform the point cloud. Users can add or subtract the triangulation from the point cloud, effectively normalizing it. Unlike some higher-level packages, there is no high-level function with names like `normalize_points()`. Instead, `pylasr` is composed of low-level atomic stages that offer more versatility.

Let's normalize the point cloud using a triangulation of the ground points (meshed DTM).

In the following example, the triangulation is used by `transform_with()` that modifies the point cloud in the pipeline. Both `triangulate()` and `transform_with()` return nothing. The output is `None`.

```python
triangulation = pylasr.triangulate(filter=["Classification == 2"])
normalization = pylasr.transform_with(triangulation, operation="-")
pipeline = triangulation + normalization
result = pipeline.execute(f)
print(result)  # {'success': True, 'data': []}
```

> **Note:** `transform_with()` can also transform with a raster and a rotation matrix. This is not presented in this tutorial.

To obtain a meaningful output, it is necessary to chain another stage. Here the point cloud has been modified but then, it is discarded because we did nothing with it. For instance, we can compute a Canopy Height Model (CHM) on the normalized point cloud. In the following pipeline, the first rasterization (`chm1`) is applied before normalization, while the second rasterization occurs after `transform_with()`, thus being applied to the transformed point cloud.

```python
triangulation = pylasr.triangulate(filter=["Classification == 2"])
normalization = pylasr.transform_with(triangulation, operation="-")
chm_before = pylasr.rasterize(res=2.0, window=2.0, operators=["max"])
chm_after = pylasr.rasterize(res=2.0, window=2.0, operators=["max"])

pipeline = chm_before + triangulation + normalization + chm_after
result = pipeline.execute(f)
```

After performing normalization, users may want to write the normalized point cloud to disk for later use. In this case, you can append the `write_las()` stage to the pipeline.

## Write LAS

`write_las()` can be called at any point in the pipeline. It writes one file per input file, using the name of the input files with added prefixes and suffixes. In the following pipeline, we read the files, write only the ground points to files named after the original files with the suffix `_ground`, perform a triangulation on the entire point cloud, followed by normalization. Finally, we write the normalized point cloud with the suffix `_normalized`.

```python
# Write ground points and normalized point cloud
write_ground = pylasr.write_las(
    ofile="/tmp/*_ground.laz", 
    filter=["Classification == 2"]
)
triangulation = pylasr.triangulate(filter=["Classification == 2"])
normalization = pylasr.transform_with(triangulation, operation="-")
write_normalized = pylasr.write_las(ofile="/tmp/*_normalized.laz")

pipeline = write_ground + triangulation + normalization + write_normalized
result = pipeline.execute(files)
```

It is crucial to include a wildcard `*` in the file path; otherwise, a single large file will be created. This behavior may be intentional. Let's consider creating a file merge pipeline. In the following example, no wildcard `*` is used for the names of the LAS/LAZ files. The input files are read, and points are sequentially written to the single file `dataset_merged.laz`, naturally forming a merge pipeline.

```python
ofile = "/tmp/dataset_merged.laz"
merge_pipeline = pylasr.reader_coverage() + pylasr.write_las(ofile)
result = merge_pipeline.execute(files)
```

## Local maximum

This stage works either on the point cloud or a raster. In the following pipeline the first stage builds a CHM, the second stage finds the local maxima in the point cloud and the third stage finds the local maxima in the CHM. `lm_points` and `lm_raster` are expected to produce relatively close results but not strictly identical.

```python
# Find local maxima in point cloud
lm_points = pylasr.local_maximum(ws=3, min_height=2.0)

# Find local maxima in CHM
chm = pylasr.rasterize(res=1.0, window=1.0, operators=["max"])
lm_raster = pylasr.local_maximum_raster(chm, ws=3, min_height=2.0)

pipeline = chm + lm_points + lm_raster
result = pipeline.execute(f)
```

## Tree Segmentation

This section presents a complex pipeline for tree segmentation using `local_maximum_raster()` to identify tree tops on a CHM. It uses `region_growing()` to segment the trees using the seeds produced by `local_maximum_raster()`. The Canopy Height Model (CHM) is triangulation-based using `triangulate()` and `rasterize_triangulation()` on the first returns. The CHM is post-processed with `pit_fill()`, an algorithm designed to enhance the CHM by filling pits and NAs. The reader may have noticed that the seeds are produced with the same raster than the one used in `region_growing()`. This is checked internally to ensure the seeds are matching the raster used for segmenting the trees.

In this tutorial, the pipeline is tested on one file to render the page faster. However, this pipeline can be applied to any number of files and will produce a continuous output, managing the buffer between files. Every intermediate output can be exported, and in this tutorial, we export everything to display all the outputs.

```python
# Triangulation of first returns for CHM
triangulation = pylasr.triangulate(filter=["ReturnNumber == 1"])
chm = pylasr.rasterize_triangulation(triangulation, res=0.5)

# Fill pits in CHM
chm_filled = pylasr.pit_fill(chm)

# Find tree tops
seeds = pylasr.local_maximum_raster(chm_filled, ws=3, min_height=2.0)

# Tree segmentation using region growing
trees = pylasr.region_growing(
    chm_filled, seeds, 
    th_tree=2.0, th_seed=0.45, th_cr=0.55, max_cr=20.0
)

pipeline = triangulation + chm + chm_filled + seeds + trees
result = pipeline.execute(f)
```

## Buffer

Point clouds are typically stored in multiple contiguous files. To avoid edge artifacts, each file must be loaded with extra points coming from neighboring files. Everything is handled automatically.

## Hulls

A Delaunay triangulation defines a convex polygon, which represents the convex hull of the points. However, in dense point clouds, removing triangles with large edges due to the absence of points results in a more complex structure.

```python
triangulation = pylasr.triangulate(max_edge=15, filter=["Classification == 2"], ofile=temp_gpkg())
result = triangulation.execute(f)
```

The `hull_triangulation()` algorithm computes the contour of the mesh, producing a concave hull with holes:

```python
triangulation = pylasr.triangulate(max_edge=15, filter=["Classification == 2"])
boundary = pylasr.hull_triangulation(triangulation)
pipeline = triangulation + boundary
result = pipeline.execute(f)
```

However `hull()` is more likely to be used without a triangulation. In this case it returns the bounding box of each point cloud file. And if it is used with `triangulate(max_edge=0)` it returns the convex hull but this is a very inefficient way to get the convex hull. (In the R tutorial this capability appears under the name `hulls()`; in Python the functions are `hull()` and `hull_triangulation()`.)

## Readers

`reader_coverage()` MUST be the first stage of each pipeline even if it can conveniently be omitted in its simplest form. However there are several readers available:

- `reader_coverage()`: will read all the files and process the entire point cloud. This is the default behavior.
- `reader_rectangles()`: will read only some rectangular regions of interest of the coverage and process them sequentially.
- `reader_circles()`: will read only some circular regions of interest of the coverage and process them sequentially.

### Reading entire coverage
```python
# Process entire dataset
pipeline = pylasr.reader_coverage() + pylasr.info()
result = pipeline.execute(files)
```

### Reading circular plots
```python
# Process circular plots (e.g., for forest inventory)
x_centers = [500000, 500100, 500200]
y_centers = [4500000, 4500100, 4500200]
radius = 11.28  # plot radius

pipeline = pylasr.reader_circles(x_centers, y_centers, radius) + \
           pylasr.summarise(metrics=["z_mean", "z_p95"])
result = pipeline.execute(files)
```

### Reading rectangular areas
```python
# Process rectangular areas
x_min, y_min = [500000, 500100], [4500000, 4500100]
x_max, y_max = [500050, 500150], [4500050, 4500150]

pipeline = pylasr.reader_rectangles(x_min, y_min, x_max, y_max) + \
           pylasr.rasterize(res=1.0, window=1.0, operators=["max"])
result = pipeline.execute(files)
```

The following pipeline triangulates the ground points, normalizes the point cloud, and computes some metric of interest **for each file of the entire coverage**. Each file is loaded with a buffer so that triangulation is performed without edge artifacts.

These readers allow building a ground inventory pipeline, or a plot extraction for examples.

## Summarise

The `summarise()` stage computes metrics of interest from the entire point cloud, i.e., all the points read. In the following example, we are processing multiple files. The stage reports the number of points, number of first returns, histograms, and other metrics.

```python
# Basic statistics
summary_pipeline = pylasr.reader_coverage() + pylasr.summarise()
result = summary_pipeline.execute(files)
print("Basic statistics:")
if result['success'] and result['data']:
    for key, value in result['data'].items():
        print(f"  {key}: {value}")

# Custom metrics
custom_metrics = pylasr.summarise(
    metrics=["z_mean", "z_p95", "i_median", "count"],
    zwbin=2.0,  # bin size for height histogram
    iwbin=50.0  # bin size for intensity histogram
)
result = custom_metrics.execute(f)
```

Like other stages, the output produced by `summarise()` depends on its positioning in the pipeline. Let's insert a sampling stage. We can see that it summarizes the point cloud in its current state in the pipeline.

```python
pipeline = pylasr.summarise() + pylasr.sampling_voxel(res=4) + pylasr.summarise()
result = pipeline.execute(f)
print("Summary before sampling:", result['data'][0] if result['data'] else None)
print("Summary after sampling:", result['data'][1] if len(result['data']) > 1 else None)
```

`summarise()` can also compute some metrics. In this case, the metrics are not computed for the entire point cloud (i.e., all the points read) but for each chunk read (i.e., each file or each query). This feature, for example, allows computing metrics for a plot inventory.

## Plot inventory

This pipeline extracts a plot inventory from a non-normalized point cloud, normalizes each plot with `transform_with()`, and computes some metrics for each plot using `summarise()`. It also writes each normalized and non-normalized plot in separate files. This means that, in a single pass, it performs the extraction, normalization, saving, and computation.

Each circular plot is loaded with a buffer to perform a correct triangulation, but all stages natively know how to handle a buffer. This means that `summarise()` computes the metrics without including buffer points and `write_las()` does not write the buffer points.

```python
# Plot coordinates
plot_centers_x = [500000, 500100, 500200]
plot_centers_y = [4500000, 4500100, 4500200]
plot_radius = 11.28

# Output files for plots
plot_files = "/tmp/plot_*.las"
plot_norm_files = "/tmp/plot_*_norm.las"

# Read circular plots
reader = pylasr.reader_circles(plot_centers_x, plot_centers_y, plot_radius)

# Normalization using ground triangulation
triangulation = pylasr.triangulate(filter=["Classification == 2"])
normalization = pylasr.transform_with(triangulation, operation="-")

# Metrics and writing
metrics = pylasr.summarise(metrics=["z_mean", "z_p95", "i_median", "count"])
write_original = pylasr.write_las(plot_files)
write_normalized = pylasr.write_las(plot_norm_files)

# Complete pipeline
pipeline = reader + write_original + triangulation + normalization + \
           write_normalized + metrics

result = pipeline.execute(files)
```

## Wildcard Usage

Usually, `write_las()` is used with a wildcard in the `ofile` argument to write one file per processed file. Otherwise, everything is written into a single massive LAS file (which might be the desired behavior). On the contrary, `rasterize()` is used without a wildcard to write everything into a single raster file, but it also accepts a wildcard to write the results in multiple files, which is very useful with `reader_circles()` to avoid having one massive raster mostly empty. Compare this pipeline with and without the wildcard.

Without a wildcard, the output is a single raster that covers the entire point cloud with patches of populated pixels:

```python
# Without wildcard - single raster covering entire coverage
ofile = "/tmp/chm.tif"  # no wildcard

x = [885100, 885100]
y = [629200, 629600]

pipeline = pylasr.reader_circles(x, y, 20) + \
           pylasr.rasterize(res=2.0, window=2.0, operators=["max"], ofile=ofile)
result = pipeline.execute(files)
```

With a wildcard, the output contains multiple rasters that cover regions of interest:

```python
# With wildcard - separate files for each plot
ofile = "/tmp/chm_*.tif"  # wildcard

x = [885100, 885100]
y = [629200, 629600]

pipeline = pylasr.reader_circles(x, y, 20) + \
           pylasr.rasterize(res=2.0, window=2.0, operators=["max"], ofile=ofile)
result = pipeline.execute(files)
```

## Stop pipeline if

A pipeline can be a long succession of stages, and it may happen that we do not want to apply the entire pipeline on every file. In this case, the `stop_if_outside` stage allows us to conditionally stop the pipeline anywhere. Let's assume we have a dataset with 100 files and a pipeline that reads, computes the hulls, and computes the DTM.

```python
hull_stage = pylasr.hull()
triangulation = pylasr.triangulate(filter=["Classification == 2"])
dtm = pylasr.rasterize_triangulation(triangulation, res=1.0)
pipeline = hull_stage + triangulation + dtm
```

It is possible to compute the hulls on the 100 files but the DTM only in a reduced region of interest defined by the user with `stop_if_outside()`.

```python
stop_condition = pylasr.stop_if_outside(880000, 620000, 885000, 630000)
pipeline = hull_stage + stop_condition + triangulation + dtm
result = pipeline.execute(files)
```

`stop_if_outside` is (currently) the only stage that can be put before `reader_coverage()`. In this case, the reading stage is skipped, effectively applying the pipeline to a subset of the files that encompass the bounding boxes defined by the user.

```python
pipeline = stop_condition + hull_stage + triangulation + dtm
result = pipeline.execute(files)
```

Notice that the pipeline below produces the same output as the pipeline above but takes longer to compute because the files that are not processed are read anyway. It is thus preferable to put `stop_if_outside()` before `reader_coverage()` in this specific case.

There are two early-termination helpers available:

1. `stop_if_outside(xmin, ymin, xmax, ymax)` – may be placed before the reader to skip reading files fully outside the AOI.
2. `stop_if_chunk_id_below(index)` – stops processing of earlier chunks (useful for resuming or partial runs). This one must appear after a reader because it needs chunk indexing.

Placing `stop_if_outside` before `reader_coverage()` is the most efficient for spatial subsetting.

## Parallel processing

The `pylasr` library supports multiple parallel processing strategies. You can configure the pipeline to use different parallelization approaches:

```python
# Sequential processing
pipeline = pylasr.rasterize(res=1.0, window=1.0, operators=["max"])
pipeline.set_sequential_strategy()

# Concurrent points strategy
pipeline.set_concurrent_points_strategy(ncores=4)

# Concurrent files strategy  
pipeline.set_concurrent_files_strategy(ncores=4)

# Nested strategy
pipeline.set_nested_strategy(ncores1=2, ncores2=2)
```

## System information and utilities

```python
# System information
print(f"Available threads: {pylasr.available_threads()}")
print(f"OpenMP support: {pylasr.has_omp_support()}")
print(f"Available RAM: {pylasr.get_available_ram()} MB")
print(f"Total RAM: {pylasr.get_total_ram()} MB")

# Check if file is spatially indexed
if pylasr.is_indexed("file.las"):
    print("File is indexed")

# Display LAS filter usage
pylasr.las_filter_usage()

# Display LAS transform usage
pylasr.las_transform_usage()
```

## Pipeline configuration

You can configure various pipeline settings:

```python
pipeline = pylasr.rasterize(res=1.0, window=1.0, operators=["max"])

# Set verbose mode
pipeline.set_verbose(True)

# Set buffer size
pipeline.set_buffer(30.0)

# Set progress display
pipeline.set_progress(True)

# Set chunk size
pipeline.set_chunk(0)  # 0 means process entire files

# Set profile output file
pipeline.set_profile_file("/tmp/profile.json")

# Check pipeline properties
print(f"Has reader: {pipeline.has_reader()}")
print(f"Pipeline string: {pipeline.to_string()}")
```

## Error handling

```python
try:
    pipeline = pylasr.rasterize(res=1.0, window=1.0, operators=["max"])
    result = pipeline.execute("nonexistent_file.las")
    
    if result['success']:
        print("Processing successful")
        print(f"Data: {result['data']}")
    else:
        print(f"Error: {result['message']}")
        
except Exception as e:
    print(f"Exception: {e}")
```

## Convenience functions

The Python bindings provide two lightweight helpers plus an empty pipeline factory:

```python
# Digital Terrain Model (TIN of ground -> raster)
dtm_pipeline = pylasr.dtm(resolution=1.0, ofile="/tmp/dtm.tif")

# Digital Surface Model (height maxima)
dsm_pipeline = pylasr.dsm(resolution=1.0, ofile="/tmp/dsm.tif")

# Execute a helper pipeline
dtm_result = dtm_pipeline.execute(f)

# Create empty pipeline for manual composition
empty_pipeline = pylasr.create_pipeline()
```

There is intentionally no dedicated CHM helper; build it explicitly:

```python
ground_tin = pylasr.triangulate(filter=["Classification == 2"])       # ground points
dtm_stage = pylasr.rasterize_triangulation(ground_tin, res=1.0)        # optional DTM output
normalize = pylasr.transform_with(ground_tin, operation="-")          # normalize heights
chm_stage = pylasr.rasterize(res=1.0, window=1.0, operators=["max"])  # canopy heights (normalized)

chm_pipeline = ground_tin + dtm_stage + normalize + chm_stage
chm_result = chm_pipeline.execute(f)
```

R users: the R vignette references a pre-recorded `normalize()` pipeline; in Python you compose it manually with `transform_with()` as above.

## Advanced usage patterns

### Chaining multiple operations
```python
# Complex processing chain (explicit variables)
ground_class = pylasr.classify_with_csf()
tin = pylasr.triangulate(filter=["Classification == 2"])  # ground triangulation
normalize = pylasr.transform_with(tin, operation="-")
sample = pylasr.sampling_voxel(res=0.5)
writer = pylasr.write_las("/tmp/processed_*.laz")

pipeline = ground_class + tin + normalize + sample + writer
result = pipeline.execute(files)
```

### Working with multiple outputs
```python
# Pipeline with multiple outputs
triangulation = pylasr.triangulate(filter=["Classification == 2"])
dtm = pylasr.rasterize_triangulation(triangulation, res=1.0, ofile="/tmp/dtm.tif")
chm = pylasr.rasterize(res=1.0, window=1.0, operators=["max"], ofile="/tmp/chm.tif")
summary = pylasr.summarise(metrics=["z_mean", "z_max", "count"])

pipeline = triangulation + dtm + chm + summary
result = pipeline.execute(files)
```

## JSON configuration files

Pipelines can also be executed from JSON configuration files:

```python
# Execute pipeline from JSON configuration
config_file = "/path/to/pipeline_config.json"
result = pylasr.execute(config_file)

# Get pipeline information from JSON
info = pylasr.pipeline_info(config_file)
print(f"Pipeline is streamable: {info.streamable}")
print(f"Pipeline reads points: {info.read_points}")
print(f"Pipeline uses buffer: {info.buffer}")
```

## Best practices

1. **Use appropriate buffer sizes** - Ensure sufficient overlap between tiles to avoid edge artifacts
2. **Choose optimal parallelization** - Test different strategies (sequential, concurrent points/files, nested) for your specific use case
3. **Memory management** - Monitor memory usage, especially with large datasets
4. **Error handling** - Always check the `success` field in results and handle errors appropriately
5. **File organization** - Use wildcards strategically to control output file organization
6. **Filtering** - Apply filters early in the pipeline to reduce processing overhead
7. **Intermediate outputs** - Save intermediate results when building complex pipelines for debugging

## Performance considerations

- Use spatial indexing (LAX files) when available
- Consider COPC (Cloud Optimized Point Cloud) format for large datasets
- Optimize chunk sizes based on available memory
- Use appropriate sampling techniques to reduce data volume when possible
- Leverage multi-threading capabilities for large processing jobs

The modular design allows users to build custom workflows ranging from simple data extraction to complex multi-step processing chains for applications like forest inventory, terrain modeling, and feature extraction.

## Conclusion

This tutorial demonstrates the core capabilities of `pylasr` for LiDAR data processing in Python. The library provides a powerful and flexible interface for creating complex point cloud processing pipelines with high performance thanks to its C++ backend. 

Key takeaways:
- Pipelines are composed of atomic stages that can be chained together
- Each stage either transforms the point cloud or produces output  
- The library handles buffering between files automatically
- Multiple parallel processing strategies are available
- Rich error handling and result reporting
- Extensive customization options for processing parameters

The modular design allows users to build custom workflows ranging from simple data extraction to complex multi-step processing chains for applications like forest inventory, terrain modeling, and feature extraction.


## Additional stages

`pylasr` has several other stages not mentioned in this tutorial. Among others:

### Point cloud operations
- `add_attribute()` - Add custom attributes to point clouds
- `add_rgb()` - Add RGB color information 
- `delete_points()` - Remove points based on filter criteria
- `edit_attribute()` - Modify existing attribute values
- `remove_attribute()` - Remove attributes from point cloud
- `sort_points()` - Sort points spatially or by attributes

### Classification and filtering
- `classify_with_sor()` - Statistical Outlier Removal for noise classification
- `classify_with_ivf()` - Isolated Voxel Filter for noise classification  
- `classify_with_csf()` - Cloth Simulation Filter for ground classification
- `filter_with_grid()` - Grid-based point filtering

### Sampling methods
- `sampling_voxel()` - Voxel-based point sampling
- `sampling_pixel()` - Pixel-based point sampling  
- `sampling_poisson()` - Poisson disk sampling

### Geometric analysis
- `geometry_features()` - Compute geometric features (eigenvalues, etc.)
- `neighborhood_metrics()` - Compute metrics in point neighborhoods

### Input/Output operations
- `write_copc()` - Write Cloud Optimized Point Cloud files
- `write_pcd()` - Write Point Cloud Data files
- `write_vpc()` - Write Virtual Point Cloud files
- `write_lax()` - Write spatial index files
- `load_raster()` - Load external raster data
- `load_matrix()` - Load transformation matrices

### Raster operations  
- `focal()` - Apply focal (neighborhood) operations on rasters
- `pit_fill()` - Fill pits and smooth raster surfaces

### Coordinate reference systems
- `set_crs()` - Set coordinate reference system using EPSG codes or WKT
# lasR 0.13.3

- Fix: #105 invalid read of extabytes
- Change: memory is reallocated and freed when many points are deleted in a stage (not visible for users)
- Change: adaptative indexation of the point cloud can speed up some process.

# lasR 0.13.2

- Fix: internal function `update_header()` updates the bounding box. Bug probably invisible to users.

# lasR 0.13.1

- Fix #103: A very silly typo bug that caused the buffering feature to be lost.
- Fix #104: crash with deprecated extrabytes in LAS format

# lasR 0.13.0

`lasR 0.13.0` is a massive rewrite of the internal engine to conform to third-party libraries licenses. With this version `lasR` is no longer tight to the LAS/LAZ format and will be able to support any point cloud format. It already partially supports the PCD file format.

I'm expecting users to encounter some bugs in the near future. However, all the unit tests are passing.

### Breaking Changes

- It is no longer possible to use `LASlib` filters such as `-drop_z_above 5` in the stages (except the reader stage). Users must use conditional commands introduced in 0.12.0, such as `"Z > 5"`.
- The reader no longer reads the bit flags from the LAS/LAZ files, as they are never used anyway.
- `sort_points()` no longer sorts by GPS time and return number. It performs a spatial sort only, and the parameter `spatial` has been removed.
- `classify_with_csf()` no longer uses the last return only.
- `write_las()` no longer preserves VLR and EVLR. This will be fixed later.
- There are likely other changes as I rewrote thousands of lines of code.

### New Features

- New: Ability to pre-read a point cloud in R using an external pointer. See `?read_cloud()`. See [the tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html).
- New: Stages can now be applied one by one to a point cloud loaded in memory.

  ```r
  f <- system.file("extdata", "Topography.las", package="lasR")
  pc <- read_cloud(f)
  u = exec(chm(5), on = pc)
  ```
- New: `reader()` replace `reader_las()` because `lasR` is not intended to be limited to LAS/LAZ. `reader_las()` is deprecated. `reader()` is supposed to support any format in the future.
- New: `reader()` has a new argument `copc_depth`  

# lasR 0.12.1

- Fix: critical bug on windows #96

# lasR 0.12.0

- Fix: In sampling stages, the filter argument previously discarded all points that did not pass the test. The updated behavior processes only the points that pass the test while leaving others untouched. For example:  
  ```r
  sampling_poisson(1, filter = keep_ground())
  ```
  Previously, this filtered the point cloud to retain only Poisson-sampled ground points. Now, it correctly Poisson-samples the ground points while preserving all other points.
- New: Added a new stage, `info()`, to print useful information about a file.  
- New: Command-line utility introduced. Users can now execute simple pipelines from the terminal. First, use `install_cmd_tools()`, then commands like these become available:  
  ```bash
  lasr info -i path/to/file.las
  lasr vpc -i path/to/folder 
  lasr lax -i path/to/folder -ncores 8
  lasr help
  lasr chm -i path/to/folder -o path/to/chm.tif -res 1 -ncore 4
  lasr dtm -i path/to/folder -o path/to/chm.tif -res 0.5 -ncore 4
  ```
- New: `transform_with()` now supports a 4x4 Affine Transformation Matrix to translate and rotate the point cloud.  
- Change: The `Eigen` library has replaced the `Armadillo` library for linear algebra. This change may affect the sign of some vectors in `geometry_feature()`.  
- New: The `filter` argument, available in many stages, now accepts programming-style strings such as `Z < 3`, `Classification == 2`, `UserData == 0`, `Intensity > 100`, `Classification %in% 2 3 4`, and `Classification %out% 0 1 2`. This approach is now the preferred way to assign filters, allowing filtering on extrabyte attributes by name, e.g., `Amplitude > 10`.  
- Change: `transform_with()` a raster (typically normalization) now performs bilinear interpolation.
- New: stages that have a `use_attribute` argument now accept any attribute including extrabytes attribute.
- New: `sampling_pixel()` gained an argument `method` and `use_attribute` to retain specific points of interest.
 
# lasR 0.10.3

Change: `normalize()` loose its argument `extrabytes` in favor of a new function `hag()` that is equivalent to `normalize(TRUE)`
New: add stages `delete_noise()`, `delete_ground()`

# lasR 0.10.2

Fix: The `local_maximum` function previously experienced significant delays when writing points to disk, taking up to 2 seconds on Linux and up to 30 seconds on Windows. This issue severely hindered parallelization capabilities. The new fix dramatically reduces the write time to around 0.1 seconds, greatly improving overall performance.

# lasR 0.10.1

- Fix #91: Resolved a critical memory addressing issue when handling very large point clouds.
- Fix: Improved pipeline efficiency by preventing the reading of buffer tiles in pipelines using `stop_if()` before `reader_las()`. Previously, the buffer was being read even when points were meant to be skipped, leading to unnecessary processing time (a few seconds per skipped file).

# lasR 0.10.0

- New: new stage `classify_with_sor()` to classify outliers with statistical outlier removal.
- New: new stage `focal()` to post-process rasters in the pipeline.
- New: new stage `filter_with_grid()` to keep the lowest or highest point per cell of a grid.
- Change: #79 raster produced on a subset of data have the minimal possible extent instead of the full extent of the point cloud.
- Fix: numerous inaccuracies in numeric parameters interpreted as integers. For example, in `geometry_feature`, `r = 5` was equal to 5 but `r = 4.5` was equal to 4.
- Fix: some parameters in `region_growing()` for individual tree segmentation were inverted.

# lasR 0.9.3

- Fix: Filters based on return number, number of returns, or classification (e.g., `-keep_last`, `-keep_class 128`) are now functional with LAS 1.4.
- Enhancement: Prevent the possibility of writing a file with the `.copc.las` extension; `.copc` is automatically discarded.
- New: The `classify_with_csf()` stage has gained a `filter` argument.

# lasR 0.9.2

- Fix #81: Added a warning if the Delaunay triangulation was not computed (fewer than 3 points).
- Fix #81: Read files with multiple Extra Bytes definitions.
- Fix #80: Circular buffers are properly removed from raster.
- Fix #83: Aborted the pipeline initialization in `load_raster()` if the raster does not have an extent that overlaps with the point cloud.
- Fix #88: `sort()` now handles duplicated gpstime properly.

# lasR 0.9.1

- Fix: Better handle datasets with overlapping tiles and non-duplicated points.
- Fix: Trigger lax indexation for a single file when processing by chunks.
- Fix: `local_maximum()` could return multiple too-close local maximum points if two close points have the exact same Z coordinates and the exact same X or Y coordinates (but not both X and Y; duplicated points were properly handled). This particularly affected `local_maximum_raster()`, where two pixels can easily have the same Z and the same X or Y.
- Fix: `region_growing()` the `th_tree` argument was not properly respected.
- Fix: `transform_with` with a TIN won't fail if there is no TIN. Instead all points will be removed.
- Fix: possible edge artifacts were possible for some filter
- Fix: #78 WGS84 VPC bounding box
- Enhance: Progress estimation display when indexing a single file.

# lasR 0.9.0

- Internal: internal changes to support visual programming. See the [lasRui project](https://github.com/r-lidar/lasRui).
- `exec()` accepts the path to a json file produced by a UI (non documented)
- New: filter `keep_ground_and_water()`
- Change: `normalize` uses `keep_ground_and_water()`
- Fix: #73 `triangulation` fails with < 3 points

# lasR 0.8.0

- New: stage `sort_points()` to optimize compression.
- New: protection against overwriting processed files while processing
- Fix: messages and warnings are thread safe. A pipeline that printed message on the console could crash. This is expected to be safe now.
- Fix: #70
- Fix: #71 `write_lax()` is 2x faster when used as a single stage
- Fix: progress bar of `write_lax()` for LAS 1.4.
- Fix: #74 `write_lax()` is now parallelized

# lasR 0.7.2

- New argument `use_gpstime` in `write_vpc()`
- Fix: division by 0 in raster stage initialization
- Fix: datetime parsing in `write_vpc()`
- Fix: `write_vpc()` writes valid files readable by QGIS

# lasR 0.7.1

- Fix: sampling stages robustly support more than 4 billions voxels
- Enhance: noise with `ivf` is faster.
- Fix: fix memory corruption for point clouds above 4.3 GB

# lasR 0.7.0

### NEW FEATURES

1. New stage `classify_with_csf()` to classify ground points.

2. The metric engine introduced in v0.6.0 can now compute metrics on extrabytes attributes. e.g. `Amplitude_mean` 

3. New stage `sampling_poisson` to perform Poisson Disk Sampling

4. `sampling_pixel` and `sampling_voxel` are faster.

### FIXES

1. `sampling_*` stages respect the `filter` argument

2. Fix #63 crash when some chunk are skipped either because they have 0 points of because of the `stop_if` stage.

3. Fix #64 metrics on RGB were actually computed on RRR


### BREAKING CHANGES

1. `classify_isolated_voxels()` renamed into `classify_with_ivf()` for consistency.

# lasR 0.6.2

- Fix: writing copc file from a copc file crashed.
- Fix: #62 attributes of the vector files were not recorded when the output file template contains a wildcard `*`
- Fix: metrics `cv` and `sd` that were not computed properly.

# lasR 0.6.1

- Fix: metrics `cv` and `sd` that return `NAs` instead of `Inf` in the edges case where they are undefined.
- Enhance: progress bar displays better the number of cores used.
- Fix: progress bar for the `reader_las()` stage. It now displays the correct percentage.
- Fix: `write_las()` without wildcard (merge mode) works with files that have different formats and scales/offsets

# lasR 0.6.0

### NEW FEATURES

1. New stage `stop_if` to conditionally escape the pipeline. New section about `stop_if` in the [online tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html).

2. New stage `write_lax`. This stage was automatically added by the engine but can now be explicitly added by users.

3. New internal "metric engine". The metric engine is used to compute metrics in, e.g., `rasterize()` with operators like `zmean`, `imean`, and so on. The metric engine has been redesigned and allows any string with the format `attribute_metric` such as `z_sd`, `i_mean`, `c_mode`, `a_mean`, `intensity_max`, `classification_mode`, `angle_mean`, and any other combinations. All attributes are mapped, and new functions are available such as `sum`, `mode`. Many more could be added easily. Former strings such as `zmean` or `imax` are no longer valid and should be replaced by `z_mean` and `i_max` but are backward compatible.

4. `rasterize` gained access to the new metric engine and can compute many more metrics natively.

5. `summarize()` gained access to the metric engine and can compute metrics for each file or each chunk. Used in conjunction with `reader_las_circle()`, it can be used, for example, to compute plot inventory metrics. The [online tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html) has been updated. The section "plot inventory" no longer uses `callback()` and is preceded by a new section "summarize".

### BREAKING CHANGES

1. The package no longer assigns `set_parallel_strategy(concurrent_points(half_core()))` when loading. Instead, if nothing is provided, this is interpreted as `concurrent_points(half_core())`. Thus, users can now write `exec(pipeline, on = file, ncores = 8)`. The engine will now respect `ncores = 8` because no global settings with global precedence were assigned. The multi-threading vignette has been updated.

2. Pipelines that include R-based stages (`rasterize` with R function, `callback`) are no longer parallelizable with the `concurrent-file` strategy. Parallelizing a pipeline that involves the R C API is terribly complex and eventually leads only to pseudo-parallelism with a lot of troubleshooting to deal with (especially to abort the pipeline). Consequently, we removed parallelism capabilities. The numerous new native metrics added in the metric engine compensate for that loss. The online documentation has been updated accordingly.

### INTERNAL CHANGES

1. A large number of changes to separate `lasR` from R. `lasR` can now be compiled as standalone software. A `Makefile` has been added to the repository. At the R level, the pipeline and the processing options are passed to the C++ engine via a JSON file instead of being passed via the R's C API, effectively separating `lasR` from R itself. The R side of `lasR` is now purely an API to the standalone engine. A JSON file produced by the `lasR` package can be executed with the standalone software: `lasr pipeline.json`. However, the syntax of the JSON file is not documented and is not intended to be documented. Rather, the JSON file should be produced by an API such as the `lasR` package, a QGIS plugin, or a Python package. Obviously, there is currently no such thing.


# lasR 0.5.6

- Fix: `reader_las()` with COPC files, depth query (`-max_depth`), and buffer. The depth query was not performed at all. The fix is temporary: it breaks the progress bar of `reader_las()` but this is a less serious bug.
- Fix: `reader_las()` with very large files.
- Fix: `load_raster()` is thread-safe
- New: `rasterize()` accepts a new argument `default_value`.  When rasterizing with an operator and a filter (e.g. `-keep_z_above 2`) some pixels that are covered by points may no longer contain any point that pass the filter criteria and are assigned NA. To differentiate NAs from non covered pixels and NAs from covered pixels but without point that pass the filter, the later case can be assigned another value such as 0.

# lasR 0.5.5

- Fix: #50 `write_vpc()` properly reprojects the bounding boxes in WGS84 
- Enhance: `write_vpc()` writes `zmin` and `zmax` for each file.
- Fix: #55 `local_maximum()` no longer fails with `ofile = ""`
- Fix: progress bar of the `reader_las()` for COPC files.
- Fix: metrics `zsd` and `isd` were incorrect due to wrong parenthesis in the code.

# lasR 0.5.4

- Fix: #48 segfault with `delete_points()` when 0 points left.
- Enhance: #47 pipelines are named `list`.
- Enhance: #47 the output `list` returned by `exec` is named and duplicated names are made unique with `make.names()`
- Doc: added some notes in the documentation of `geometry_features()` to address question in #45
- Enhance: #49 `set_crs()` no longer forces the pipeline to read the files.
- Enhance: `exec()` normalizes the path so users do not get an error when providing a path with a `~`.
- New: `rasterize()` gained a metric `zaboveX` to compute canopy cover.

# lasR 0.5.3

- Fix: #45 computation time of `geometry_features` after `delete_points()`
- Fix: `local_maximum()` was processing deleted points.
- Enhance: #44 `write_vpc` write the `datetime`
- Enhance: `delete_points` can now physically remove the deleted points if the number of points deleted is important. Before they were flagged but kept in memory. It can also free available memory.

# lasR 0.5.2

- New: #42 `write_vpc()` gained an argument `absolute_path`
- Fix: #42 `write_vpc()` orders the long/lat coordinates properly on Linux
- Fix: #42 `write_vpc()` writes the absolute path of the relative path does not exist on Windows
- Fix: #40 `triangulate()` with 0 point chunk. 
- Fix: #43: `geometry_feature` works if the file already contains some extrabytes attributes
- Enhance: 0 point point-clouds are no longer stopping the computation. If a stage such as `delete_points()` removes all the points, the pipeline is stopped for the current chunk/file but the computation keep going for others. This was not the case for all stages and some stages could either crash or stop the computation.

# lasR 0.5.1

- Fix: `write_vpc()` does not crash with files without CRS
- Fix: `write_vpc()` write the CRS set upstream by `set_crs()`

# lasR 0.5.0

- New: stage `geometry_features()` to compute point wise geometry features based on k-nearest neighbors.
- New: stage `callback()` can load more than 10 extrabyte attributes. Using the flag `E` all the extrabytes are loaded.
- New: stage `set_crs()` to assign a coordinate reference system at some point of the pipeline.
- New: raster in `GeoTiff` format are now created with `COMPRESS=DEFLATE`, `PREDICTOR=2`,`TILED=YES` effectively reducing the size of the rasters
- New: `summarize()` output includes the CRS.

# lasR 0.4.8

- Enhance: #33 `local_maximum()` gained a `record_attributes` argument to chose if the attribute of the points are recorded in the vector file.
- Enhance: #33 `local_maximum_raster()` no longer record zeroed LAS point attributes

# lasR 0.4.7

- Fix: #32 writing a vector file with a path containing a wildcard crashed the program.

# lasR 0.4.6

- Fix: `lax` included into `laz` file were not working.
- Fix: #30 can read files bigger than 2.14 GB

# lasR 0.4.5

- Fix: #29 using a filter in `rasterize()` produced corrupted output.

# lasR 0.4.4

- Fix: bug with `set_parallel_strategy(nested(ncores = 4, ncores2 = 4))`.
- Fix: attribute `datatime` is `datetime` in VPC files.
- Fix: #25 triangulation with 0 points crashed. 0 points are possible with a filter.
- Fix: #24 `write_vpc()` writes the correct number of points for LAS 1.4 files.
- Fix: read of WKT strings in LAS files with a size inferior to what was declared in the header (null-terminated before `record_length_after_header`).

# lasR 0.4.3

- Fix: #22 segfault with partial processing.
- Fix: memory access to WKT strings non-null-terminated.

# lasR 0.4.2

- Fix: `add_attribute()` was incorrectly reallocating memory causing potential crashes, especially when adding several attributes.
- Fix: `reader_las()` crashing if the header of the LAS file did not record the correct number of points.
- Fix: naming of the queries.
- Documentation: reorganized the URLs and navbar of the website.

# las 0.4.1

- Fix: #20 segfault with `local_maximum_raster`.
- Fix: segfault when reading LAS files with a header that incorrectly reports the number of points.

# lasR 0.4.0

- New: parallelism on multiple files. See `?multithreading`
- New: stage `local_maximum_raster` to compute local maximum on a raster
- New: argument `with` in `exec` to pass processing options that should be preferred over direct naming.
- New: function `set_exec_options()` to assign global processing options and override arguments potentially hardcoded in `exec()`
- New: stage `load_raster` to read a raster instead of producing it on the fly from the point cloud.
- New: stage `add_rgb` to modify the point data format
- Doc: new article on the website about parallelism for an illustrated version of `?multithreading`
- Doc: improve documentation about processing options in `?exec` and `?set_exec_options`

# lasR 0.3.6

- Fix: #18 strongly improving arithmetic accuracy in `point_in_triangle`.

# lasR 0.3.5

- Fix: #17 `transform_with` can be used with `pit_fill`

# lasR 0.3.4

- Fix: #15 `pit_fill` producing corrupted output
- Fix: `pit_fill` was not respecting the parameters given by the user
- Fix: `pit_fill` in combination with `rasterize("max")` was not working properly

# lasR 0.3.3

- Fix: #12 write lax with buffered chunk
- Fix: #13 processing by chunk was not buffered

# lasR 0.3.2

- Fix: CRS are working on Windows
- Fix: `library(lasR)` transparently checks for latest version on Windows.

# lasR 0.3.1

- Fix: bugs when making a spatial query on multiple files with multiple spatial indexing systems (e.g. lax+nothing, lax+copc)

# lasR 0.3.0

- Change: `processor()` and `reader()` are deprecated and are replaced by `exec()` and `reader_las()`. This intends to provide a more consistent and natural way to separate the pipeline. i.e the stages and the global processing options i.e. the buffer, the chunking, the progress bar. For example the following now respects the `LAScatalog` processing options and this was not possible with the previous syntax.
  ```r
  ctg = lidR::readLAScatalog()
  pipeline = reader_las() + rasterize(...)
  exec(pipeline, on = ctg)
  ```
- New: the processor is now able to process by chunk like `lidR`
  ```r
  pipeline = reader_las() + rasterize(...)
  exec(pipeline, on = file, chunk = 500)
  ```
- New: stage `delete_points()` to remove some points in the pipeline.
- New: it is now possible to write the following:
  ```r
  dtm = dtm()
  pipeline <- read + dtm + transform_with(dtm[[2]])
  ```
- New: it is not possible to omit the reader stage. It automatically adds a default reader
  ```r
  pipeline = rasterize(...)
  exec(pipeline, on = ctg)
  ```
- New: triangulation is 4x faster and uses half the memory.
- Fix: `summarize()`, `rasterize()` and `write_las()` no longer process withheld points in streaming mode.

# lasR 0.2.1 (2024-03-05)

- Fix: `callback()` properly handles errors from the injected function
- New: handy functions `tempxyz()` to generate temp files with extension `.xyz`.
- New: `rasterize()` is now parallelized with internal metrics including for buffered area based approach
- New: `rasterize()` gained a progress bar with internal metrics.

# lasR 0.2.0 (2024-03-01)

- New: `rasterize()` gains the ability to perform a multi-resolution or buffered rasterization. See documentation.
- New: `rasterize()` gains numerous native metrics such as `zmax`, `zmean`, `zmedian`, `imax`, `imean` and so on.
- New: the internal engine gains the ability to skip the processing of some files of the collection and use these files only to load a buffer. This feature works with a `LAScatalog` from `lidR` respecting the `processed` attribute used in `lidR`
- Fix: loading the package being offline created a bug were R no longer handles errors.

# lasR 0.1.2 (2024-02-10)

- New: progress bar when reading the header of the files (`LAScatalog`) can be enabled with `progress = TRUE`
- Fix: progress bar starts to appear earlier i.e. from 0%. For some pipeline it affects the feeling of progress.

# lasR 0.1.1 (2024-02-08)

- Doc: Corrected the documentation for the argument `ncores` in `processor()`, which incorrectly mentioned that it was not supported.
- New: Added new functions `ncores()` and `half_cores()`.
- Fix: Corrected the reader progress bar display when reading a las file with a filter and a buffer.
- Fix: Fixed the overall progress bar, which was delayed by one file and was showing incorrect progress.

# lasR 0.1.0 (2024-02-01)

- Open to public
- Fix: Fix the overall progress bar, which was delayed by one file and was showing incorrect progress.

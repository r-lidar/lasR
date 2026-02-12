# Changelog

## lasR 0.18.0

- New: [\#263](https://github.com/r-lidar/lasR/issues/263) new argument
  `always_up` in geometry_features().

## lasR 0.17.4

- Fix: [\#247](https://github.com/r-lidar/lasR/issues/247) memory
  violation in some edge cases.
- Fix: [\#259](https://github.com/r-lidar/lasR/issues/259) incorrect
  values reported for normal vectors.

## lasR 0.17.3

- Fix: [\#213](https://github.com/r-lidar/lasR/issues/213) VPC 3D
  geometries.
- Fix: [\#217](https://github.com/r-lidar/lasR/issues/217) invalid
  triangulations
- Fix: [\#219](https://github.com/r-lidar/lasR/issues/219)
  [`hulls()`](https://r-lidar.github.io/lasR/reference/hulls.md) is
  computed more robustly with GEOS.

## lasR 0.17.2

- Fix: [\#198](https://github.com/r-lidar/lasR/issues/198) LAS files
  with 0 points are discarded on-the-fly.

- New: argument `store_in_attribute` in `local_maximum`

- New: *undocumented* capacity at logging informations in files (log,
  progress). Accessible in R API via

  ``` r
  exec(..., progress_file = "path/to/progress.ext", log_file = "path/to/log.ext")
  ```

  Accessible in the C++ API via two members

  ``` r
  Pipeline::set_progress_file(std::string);
  Pipeline::set_profile_file(std::string);
  ```

- Fix: [\#205](https://github.com/r-lidar/lasR/issues/205) absolute
  paths in `write_vpc` were broken since 0.17.0

- Fix: [\#207](https://github.com/r-lidar/lasR/issues/207) `sor` when
  executed in parallel on multiple files.

- Fix: [\#206](https://github.com/r-lidar/lasR/issues/206)
  [`local_maximum()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  after [`normalize()`](https://r-lidar.github.io/lasR/reference/hag.md)
  or
  [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md)

## lasR 0.17.1

- Fix: [`info()`](https://r-lidar.github.io/lasR/reference/info.md)
  prints informations for each processed chunk. Not only the first one.
- Fix: [\#196](https://github.com/r-lidar/lasR/issues/196). ExtraBytes
  attributes were zeroed when reading two or more files (main file +
  buffer file)

## lasR 0.17.0

`lasR 0.17.0` does not bring new features. However it has been
redesigned internally to provided a C++ API. The R API (the `lasR`
package) now uses the C++ API. And we have now a `python` package
(`pylasr`) that leverage the C++ API as well. It is now possible to
integrate `lasR` into your own API in `matlab`, `julia`or any language
that supports a C++ binding

Below a simple pipeline with 3 stage using the `C++`, `R` and python
`API`. The variable `on` is a list of file on which to apply the
pipeline.

in `C++`:

``` cpp
#include "api.h"

using namespace api;

// platform independent tmp file
std::filesystem::path temp_dir = std::filesystem::temp_directory_path(); 
std::filesystem::path temp_file = temp_dir / "test.las";

Pipeline p;
p.set_files(on);
p.set_concurrent_files_strategy(8);
p.set_progress(true);

p += info() +
  delete_points({"Z < 1.37"}) + 
  write_las(temp_file);

std::string file = p.write_json();

execute(file);
```

in `R`:

``` r
library(lasR)

pipeline = info() + 
   delete_points("Z < 1.37") +
   write_las()

execute(pipeline, on, ncores = 8, progress = TRUE);
```

in `python`:

``` py
import pylasr

# Use example file from the package
example_file = "inst/extdata/Example.las"
output_file = "filtered_output.las"

pipeline = (pylasr.info() + 
            pylasr.delete_points(["Z < 1.37"]) + 
            pylasr.write_las(output_file))

pipeline.set_progress(True)
result = pipeline.execute(example_file)
```

### Breaking Changes

- **Inverted `delete_points` behavior**: The stage `delete_points` now
  works consistently with other stages’ `filter` arguments. Previously,
  `delete_points("Z < 4")` kept points below 4, which allowed for
  commands like `delete_points(keep_z_below(4))` but was
  counter-intuitive.Now, points matching the filter are *processed*. In
  the case of
  [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md),
  this means **deleted**.

### Fixes

- Fixed a crash in `geometry_features` when running after points had
  been deleted by
  [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md).

## lasR 0.16.2

- Fix [\#164](https://github.com/r-lidar/lasR/issues/164): `lasR` is now
  as fast as it should be. For an unknown reason, it had become
  extremely slow with some stages involving spatial queries such as
  [`normalize()`](https://r-lidar.github.io/lasR/reference/hag.md),
  [`transform_with()`](https://r-lidar.github.io/lasR/reference/transform_with.md),
  `sor()`, and `geometry_feature()`. `lasR` performance has been
  restored.
- Enhancement: building a `kdtree` for spatial indexing can take
  significant time (several seconds). Spatial indexes are now built only
  when required. Thus, a pipeline using only
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  will no longer build a spatial index.
- Regression: we regained the full speed of `lasR` at the cost of
  increased memory usage. `lasR` now consumes more memory.

## lasR 0.16.1

- Fix [\#158](https://github.com/r-lidar/lasR/issues/158): triangulation
  with fewer than 3 points
- Fix [\#160](https://github.com/r-lidar/lasR/issues/160): crash with
  empty folder
- Fix: the NIR attribute is now recognized as part of the LAS
  specification
- Fix [\#161](https://github.com/r-lidar/lasR/issues/161):
  [`reader_circles()`](https://r-lidar.github.io/lasR/reference/reader.md)
  and
  [`reader_rectangles()`](https://r-lidar.github.io/lasR/reference/reader.md)
  are skipping queries outside the file collection
- Fix [\#166](https://github.com/r-lidar/lasR/issues/166):
  [`sort_points()`](https://r-lidar.github.io/lasR/reference/sort_points.md)
  works with deleted points in previous stages
- Fix [\#170](https://github.com/r-lidar/lasR/issues/170):
  [`classify_with_sor()`](https://r-lidar.github.io/lasR/reference/classify_with_sor.md)
  is flagged as parallelized and should use multiple cores
- Fix [\#170](https://github.com/r-lidar/lasR/issues/170): KNN search
  has been rewritten using `nanoflann`; stages relying on KNN search are
  now much faster

## lasR 0.16.0

- New: stage
  [`edit_attribute()`](https://r-lidar.github.io/lasR/reference/edit_attribute.md)
- New: stage
  [`remove_attribute()`](https://r-lidar.github.io/lasR/reference/add_attribute.md)
- New: filters
  [`keep_z_between()`](https://r-lidar.github.io/lasR/reference/filters.md)
  and
  [`drop_z_between()`](https://r-lidar.github.io/lasR/reference/filters.md)
- Doc: enhanced documentation for the `filter` argument
- Doc: revised documentation for
  [`add_extrabytes()`](https://r-lidar.github.io/lasR/reference/add_attribute.md)
- Change: `add_attributes()` checks for reserved names
  ([\#159](https://github.com/r-lidar/lasR/issues/159))
- Change: the angle computed by `geometry_feature()` is now called
  `inclination` instead of `angle`, as `angle` is a reserved name

## lasR 0.15.1

- Fix [\#146](https://github.com/r-lidar/lasR/issues/146) fix memory
  layout after adding new attributes
- Fix [\#146](https://github.com/r-lidar/lasR/issues/146)
  `add_attribute()` overwrite previous attribute rather that duplicating
  them.
- Fix [\#135](https://github.com/r-lidar/lasR/issues/135) write valid
  WKT string in COPC files
- Fix [\#151](https://github.com/r-lidar/lasR/issues/151) memory
  corruption when calling `callback` with deleted points

## lasR 0.15.0

- New: `lasR` now supports the PCD format. The
  [`reader()`](https://r-lidar.github.io/lasR/reference/reader.md)
  function can read PCD files, and a new stage,
  [`write_pcd()`](https://r-lidar.github.io/lasR/reference/write.md), is
  available. However, due to the current state of the software and
  limitations of the format itself, functionality is restricted: `lasR`
  cannot read multiple PCD files, and thus cannot buffer or merge them.
  Additionally,
  [`write_pcd()`](https://r-lidar.github.io/lasR/reference/write.md)
  does not support streaming data.
- New: [\#140](https://github.com/r-lidar/lasR/issues/140)
  [`info()`](https://r-lidar.github.io/lasR/reference/info.md) now
  prints useful COPC metadata.
- Fix: [\#142](https://github.com/r-lidar/lasR/issues/142) circular
  buffers are now handled properly.
- Change: [\#139](https://github.com/r-lidar/lasR/issues/139) the
  [`chm()`](https://r-lidar.github.io/lasR/reference/dsm.md) function
  has been replaced by
  [`dsm()`](https://r-lidar.github.io/lasR/reference/dsm.md).

## lasR 0.14.1

- Fix: [\#141](https://github.com/r-lidar/lasR/issues/141)
  [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md)
  drops circular buffers properly
- Fix: [\#136](https://github.com/r-lidar/lasR/issues/136)
  [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md)
  preserves dates and writes generating software
- Enhance: [\#137](https://github.com/r-lidar/lasR/issues/137) a
  pipeline preserve VLR attributes of a LAS files

## lasR 0.14.0

- Doc: Documented that lasR can write COPC files.
- New: Added
  [`write_copc()`](https://r-lidar.github.io/lasR/reference/write.md)
  function with extra arguments to control hierarchy depth and density.
- Fix: Fixed unmapped memory issue when writing COPC files.

## lasR 0.13.9

- Fix: [\#113](https://github.com/r-lidar/lasR/issues/113)
  `geometry_feature()` overwrite attributes if they are already
  existing.
- Fix: a very serious bug where memory may be corrupted after deleting
  points leading to unexpected results or crash.

## lasR 0.13.8

- New argument `check` in
  [`load_matrix()`](https://r-lidar.github.io/lasR/reference/load_matrix.md)
  to disable orthogonality check
- Enhancement:
  [`info()`](https://r-lidar.github.io/lasR/reference/info.md) is
  reporting the point density.
- Enhancement: incorrect extension for raster of vector files outputs
  now returns a helpful error. Previously using e.g. `.tif` for a local
  maximum output instead of `.shp`or `gpkg` returned a non helpful
  error: *Erreur : error 1 while creating GDAL dataset. Attempt to
  create 0x0 dataset is illegal,sizes must be larger than zero.* It now
  returns *Expecting a vector format for: file.tif*

## lasR 0.13.7

- Fix: [\#123](https://github.com/r-lidar/lasR/issues/123) `filter`
  argument with negative numbers
- Fix: [\#124](https://github.com/r-lidar/lasR/issues/124)
  [`exec()`](https://r-lidar.github.io/lasR/reference/exec.md) with 0
  file no longer crashes.
- New: new stage
  [`load_matrix()`](https://r-lidar.github.io/lasR/reference/load_matrix.md)

## lasR 0.13.6

- Fix: [\#120](https://github.com/r-lidar/lasR/issues/120) fix
  [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md)
  properly writes return number and number of return in LAS 1.4 format 6
- Fix: [`info()`](https://r-lidar.github.io/lasR/reference/info.md) no
  longer prints for every point in streamable pipelines
- Enhancement: better support of LAS format in
  [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md). It
  will write the same LAS file version and format than the input.
- Enhancement:
  [`info()`](https://r-lidar.github.io/lasR/reference/info.md) prints
  the source format (`LAS`, `PCD`) and its version (`1.4`, `0.7`).

## lasR 0.13.5

- New: in `transform_with` when used with a raster, bilinear
  interpolation can be deactivated
- Fix: [\#118](https://github.com/r-lidar/lasR/issues/118) one pixel
  shift in the DTM alignment

## lasR 0.13.4

- New: Added metrics `skew` and `kurt` in the metric engine.
- New: \[#110\] For LAS files, the bit flags are now read as bit
  attributes. This feature was lost in version 0.13.0.
- New: \[#110\]
  [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md) now
  automatically writes LAS 1.4 format if required.
- Fix: Resolved floating-point inaccuracy in
  [`region_growing()`](https://r-lidar.github.io/lasR/reference/region_growing.md)
  that could cause edge effects across two tiles or chunks effectively
  splitting some trees in two parts with different IDs in
  high-resolution CHM.
- Fix: The documentation for
  [`region_growing()`](https://r-lidar.github.io/lasR/reference/region_growing.md)
  states that the `max_cr` parameter represents the *“Maximum value of
  the crown **diameter** of a detected tree”*. In practice, it was used
  as the maximum **radius**, resulting in trees larger than expected.
  This has been corrected.

## lasR 0.13.3

- Fix: [\#105](https://github.com/r-lidar/lasR/issues/105) invalid read
  of extrabytes
- Change: memory is reallocated and freed when many points are deleted
  in a stage (not visible for users)
- Change: adaptive indexation of the point cloud should speed up some
  process for some low or high density point clouds.

## lasR 0.13.2

- Fix: internal function `update_header()` updates the bounding box. Bug
  probably invisible to users.

## lasR 0.13.1

- Fix [\#103](https://github.com/r-lidar/lasR/issues/103): A very silly
  typo bug that caused the buffering feature to be lost.
- Fix [\#104](https://github.com/r-lidar/lasR/issues/104): crash with
  deprecated extrabytes in LAS format

## lasR 0.13.0

`lasR 0.13.0` is a massive rewrite of the internal engine to conform to
third-party libraries licenses. With this version `lasR` is no longer
tight to the LAS/LAZ format and will be able to support any point cloud
format. It already partially supports the PCD file format.

I’m expecting users to encounter some bugs in the near future. However,
all the unit tests are passing.

#### Breaking Changes

- It is no longer possible to use `LASlib` filters such as
  `-drop_z_above 5` in the stages (except the reader stage). Users must
  use conditional commands introduced in 0.12.0, such as `"Z > 5"`.
- The reader no longer reads the bit flags from the LAS/LAZ files, as
  they are never used anyway.
- [`sort_points()`](https://r-lidar.github.io/lasR/reference/sort_points.md)
  no longer sorts by GPS time and return number. It performs a spatial
  sort only, and the parameter `spatial` has been removed.
- [`classify_with_csf()`](https://r-lidar.github.io/lasR/reference/classify_with_csf.md)
  no longer uses the last return only.
- [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md) no
  longer preserves VLR and EVLR. This will be fixed later.
- There are likely other changes as I rewrote thousands of lines of
  code.

#### New Features

- New: Ability to pre-read a point cloud in R using an external pointer.
  See `?read_cloud()`. See [the
  tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html).

- New: Stages can now be applied one by one to a point cloud loaded in
  memory.

  ``` r
  f <- system.file("extdata", "Topography.las", package="lasR")
  pc <- read_cloud(f)
  u = exec(chm(5), on = pc)
  ```

- New: [`reader()`](https://r-lidar.github.io/lasR/reference/reader.md)
  replace
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  because `lasR` is not intended to be limited to LAS/LAZ.
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  is deprecated.
  [`reader()`](https://r-lidar.github.io/lasR/reference/reader.md) is
  supposed to support any format in the future.

- New: [`reader()`](https://r-lidar.github.io/lasR/reference/reader.md)
  has a new argument `copc_depth`

## lasR 0.12.1

- Fix: critical bug on windows
  [\#96](https://github.com/r-lidar/lasR/issues/96)

## lasR 0.12.0

- Fix: In sampling stages, the filter argument previously discarded all
  points that did not pass the test. The updated behavior processes only
  the points that pass the test while leaving others untouched. For
  example:

  ``` r
  sampling_poisson(1, filter = keep_ground())
  ```

  Previously, this filtered the point cloud to retain only
  Poisson-sampled ground points. Now, it correctly Poisson-samples the
  ground points while preserving all other points.

- New: Added a new stage,
  [`info()`](https://r-lidar.github.io/lasR/reference/info.md), to print
  useful information about a file.  

- New: Command-line utility introduced. Users can now execute simple
  pipelines from the terminal. First, use
  [`install_cmd_tools()`](https://r-lidar.github.io/lasR/reference/install_cmd_tools.md),
  then commands like these become available:

  ``` bash
  lasr info -i path/to/file.las
  lasr vpc -i path/to/folder 
  lasr lax -i path/to/folder -ncores 8
  lasr help
  lasr chm -i path/to/folder -o path/to/chm.tif -res 1 -ncore 4
  lasr dtm -i path/to/folder -o path/to/chm.tif -res 0.5 -ncore 4
  ```

- New:
  [`transform_with()`](https://r-lidar.github.io/lasR/reference/transform_with.md)
  now supports a 4x4 Affine Transformation Matrix to translate and
  rotate the point cloud.  

- Change: The `Eigen` library has replaced the `Armadillo` library for
  linear algebra. This change may affect the sign of some vectors in
  `geometry_feature()`.  

- New: The `filter` argument, available in many stages, now accepts
  programming-style strings such as `Z < 3`, `Classification == 2`,
  `UserData == 0`, `Intensity > 100`, `Classification %in% 2 3 4`, and
  `Classification %out% 0 1 2`. This approach is now the preferred way
  to assign filters, allowing filtering on extrabyte attributes by name,
  e.g., `Amplitude > 10`.  

- Change:
  [`transform_with()`](https://r-lidar.github.io/lasR/reference/transform_with.md)
  a raster (typically normalization) now performs bilinear
  interpolation.

- New: stages that have a `use_attribute` argument now accept any
  attribute including extrabytes attribute.

- New:
  [`sampling_pixel()`](https://r-lidar.github.io/lasR/reference/sampling.md)
  gained an argument `method` and `use_attribute` to retain specific
  points of interest.

## lasR 0.10.3

Change: [`normalize()`](https://r-lidar.github.io/lasR/reference/hag.md)
loose its argument `extrabytes` in favor of a new function
[`hag()`](https://r-lidar.github.io/lasR/reference/hag.md) that is
equivalent to `normalize(TRUE)` New: add stages
[`delete_noise()`](https://r-lidar.github.io/lasR/reference/delete_points.md),
[`delete_ground()`](https://r-lidar.github.io/lasR/reference/delete_points.md)

## lasR 0.10.2

Fix: The `local_maximum` function previously experienced significant
delays when writing points to disk, taking up to 2 seconds on Linux and
up to 30 seconds on Windows. This issue severely hindered
parallelization capabilities. The new fix dramatically reduces the write
time to around 0.1 seconds, greatly improving overall performance.

## lasR 0.10.1

- Fix [\#91](https://github.com/r-lidar/lasR/issues/91): Resolved a
  critical memory addressing issue when handling very large point
  clouds.
- Fix: Improved pipeline efficiency by preventing the reading of buffer
  tiles in pipelines using `stop_if()` before
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md).
  Previously, the buffer was being read even when points were meant to
  be skipped, leading to unnecessary processing time (a few seconds per
  skipped file).

## lasR 0.10.0

- New: new stage
  [`classify_with_sor()`](https://r-lidar.github.io/lasR/reference/classify_with_sor.md)
  to classify outliers with statistical outlier removal.
- New: new stage
  [`focal()`](https://r-lidar.github.io/lasR/reference/focal.md) to
  post-process rasters in the pipeline.
- New: new stage
  [`filter_with_grid()`](https://r-lidar.github.io/lasR/reference/filter_with_grid.md)
  to keep the lowest or highest point per cell of a grid.
- Change: [\#79](https://github.com/r-lidar/lasR/issues/79) raster
  produced on a subset of data have the minimal possible extent instead
  of the full extent of the point cloud.
- Fix: numerous inaccuracies in numeric parameters interpreted as
  integers. For example, in `geometry_feature`, `r = 5` was equal to 5
  but `r = 4.5` was equal to 4.
- Fix: some parameters in
  [`region_growing()`](https://r-lidar.github.io/lasR/reference/region_growing.md)
  for individual tree segmentation were inverted.

## lasR 0.9.3

- Fix: Filters based on return number, number of returns, or
  classification (e.g., `-keep_last`, `-keep_class 128`) are now
  functional with LAS 1.4.
- Enhancement: Prevent the possibility of writing a file with the
  `.copc.las` extension; `.copc` is automatically discarded.
- New: The
  [`classify_with_csf()`](https://r-lidar.github.io/lasR/reference/classify_with_csf.md)
  stage has gained a `filter` argument.

## lasR 0.9.2

- Fix [\#81](https://github.com/r-lidar/lasR/issues/81): Added a warning
  if the Delaunay triangulation was not computed (fewer than 3 points).
- Fix [\#81](https://github.com/r-lidar/lasR/issues/81): Read files with
  multiple Extra Bytes definitions.
- Fix [\#80](https://github.com/r-lidar/lasR/issues/80): Circular
  buffers are properly removed from raster.
- Fix [\#83](https://github.com/r-lidar/lasR/issues/83): Aborted the
  pipeline initialization in
  [`load_raster()`](https://r-lidar.github.io/lasR/reference/load_raster.md)
  if the raster does not have an extent that overlaps with the point
  cloud.
- Fix [\#88](https://github.com/r-lidar/lasR/issues/88):
  [`sort()`](https://rdrr.io/r/base/sort.html) now handles duplicated
  gpstime properly.

## lasR 0.9.1

- Fix: Better handle datasets with overlapping tiles and non-duplicated
  points.
- Fix: Trigger lax indexation for a single file when processing by
  chunks.
- Fix:
  [`local_maximum()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  could return multiple too-close local maximum points if two close
  points have the exact same Z coordinates and the exact same X or Y
  coordinates (but not both X and Y; duplicated points were properly
  handled). This particularly affected
  [`local_maximum_raster()`](https://r-lidar.github.io/lasR/reference/local_maximum.md),
  where two pixels can easily have the same Z and the same X or Y.
- Fix:
  [`region_growing()`](https://r-lidar.github.io/lasR/reference/region_growing.md)
  the `th_tree` argument was not properly respected.
- Fix: `transform_with` with a TIN won’t fail if there is no TIN.
  Instead all points will be removed.
- Fix: possible edge artifacts were possible for some filter
- Fix: [\#78](https://github.com/r-lidar/lasR/issues/78) WGS84 VPC
  bounding box
- Enhance: Progress estimation display when indexing a single file.

## lasR 0.9.0

- Internal: internal changes to support visual programming. See the
  [lasRui project](https://github.com/r-lidar/lasRui).
- [`exec()`](https://r-lidar.github.io/lasR/reference/exec.md) accepts
  the path to a json file produced by a UI (non documented)
- New: filter
  [`keep_ground_and_water()`](https://r-lidar.github.io/lasR/reference/filters.md)
- Change: `normalize` uses
  [`keep_ground_and_water()`](https://r-lidar.github.io/lasR/reference/filters.md)
- Fix: [\#73](https://github.com/r-lidar/lasR/issues/73) `triangulation`
  fails with \< 3 points

## lasR 0.8.0

- New: stage
  [`sort_points()`](https://r-lidar.github.io/lasR/reference/sort_points.md)
  to optimize compression.
- New: protection against overwriting processed files while processing
- Fix: messages and warnings are thread safe. A pipeline that printed
  message on the console could crash. This is expected to be safe now.
- Fix: [\#70](https://github.com/r-lidar/lasR/issues/70)
- Fix: [\#71](https://github.com/r-lidar/lasR/issues/71)
  [`write_lax()`](https://r-lidar.github.io/lasR/reference/write_lax.md)
  is 2x faster when used as a single stage
- Fix: progress bar of
  [`write_lax()`](https://r-lidar.github.io/lasR/reference/write_lax.md)
  for LAS 1.4.
- Fix: [\#74](https://github.com/r-lidar/lasR/issues/74)
  [`write_lax()`](https://r-lidar.github.io/lasR/reference/write_lax.md)
  is now parallelized

## lasR 0.7.2

- New argument `use_gpstime` in
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
- Fix: division by 0 in raster stage initialization
- Fix: datetime parsing in
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
- Fix:
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  writes valid files readable by QGIS

## lasR 0.7.1

- Fix: sampling stages robustly support more than 4 billions voxels
- Enhance: noise with `ivf` is faster.
- Fix: fix memory corruption for point clouds above 4.3 GB

## lasR 0.7.0

#### NEW FEATURES

1.  New stage
    [`classify_with_csf()`](https://r-lidar.github.io/lasR/reference/classify_with_csf.md)
    to classify ground points.

2.  The metric engine introduced in v0.6.0 can now compute metrics on
    extrabytes attributes. e.g. `Amplitude_mean`

3.  New stage `sampling_poisson` to perform Poisson Disk Sampling

4.  `sampling_pixel` and `sampling_voxel` are faster.

#### FIXES

1.  `sampling_*` stages respect the `filter` argument

2.  Fix [\#63](https://github.com/r-lidar/lasR/issues/63) crash when
    some chunk are skipped either because they have 0 points of because
    of the `stop_if` stage.

3.  Fix [\#64](https://github.com/r-lidar/lasR/issues/64) metrics on RGB
    were actually computed on RRR

#### BREAKING CHANGES

1.  `classify_isolated_voxels()` renamed into
    [`classify_with_ivf()`](https://r-lidar.github.io/lasR/reference/classify_with_ivf.md)
    for consistency.

## lasR 0.6.2

- Fix: writing copc file from a copc file crashed.
- Fix: [\#62](https://github.com/r-lidar/lasR/issues/62) attributes of
  the vector files were not recorded when the output file template
  contains a wildcard `*`
- Fix: metrics `cv` and `sd` that were not computed properly.

## lasR 0.6.1

- Fix: metrics `cv` and `sd` that return `NAs` instead of `Inf` in the
  edges case where they are undefined.
- Enhance: progress bar displays better the number of cores used.
- Fix: progress bar for the
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  stage. It now displays the correct percentage.
- Fix:
  [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md)
  without wildcard (merge mode) works with files that have different
  formats and scales/offsets

## lasR 0.6.0

#### NEW FEATURES

1.  New stage `stop_if` to conditionally escape the pipeline. New
    section about `stop_if` in the [online
    tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html).

2.  New stage `write_lax`. This stage was automatically added by the
    engine but can now be explicitly added by users.

3.  New internal “metric engine”. The metric engine is used to compute
    metrics in, e.g.,
    [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
    with operators like `zmean`, `imean`, and so on. The metric engine
    has been redesigned and allows any string with the format
    `attribute_metric` such as `z_sd`, `i_mean`, `c_mode`, `a_mean`,
    `intensity_max`, `classification_mode`, `angle_mean`, and any other
    combinations. All attributes are mapped, and new functions are
    available such as `sum`, `mode`. Many more could be added easily.
    Former strings such as `zmean` or `imax` are no longer valid and
    should be replaced by `z_mean` and `i_max` but are backward
    compatible.

4.  `rasterize` gained access to the new metric engine and can compute
    many more metrics natively.

5.  `summarize()` gained access to the metric engine and can compute
    metrics for each file or each chunk. Used in conjunction with
    `reader_las_circle()`, it can be used, for example, to compute plot
    inventory metrics. The [online
    tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html) has
    been updated. The section “plot inventory” no longer uses
    [`callback()`](https://r-lidar.github.io/lasR/reference/callback.md)
    and is preceded by a new section “summarize”.

#### BREAKING CHANGES

1.  The package no longer assigns
    `set_parallel_strategy(concurrent_points(half_core()))` when
    loading. Instead, if nothing is provided, this is interpreted as
    `concurrent_points(half_core())`. Thus, users can now write
    `exec(pipeline, on = file, ncores = 8)`. The engine will now respect
    `ncores = 8` because no global settings with global precedence were
    assigned. The multi-threading vignette has been updated.

2.  Pipelines that include R-based stages (`rasterize` with R function,
    `callback`) are no longer parallelizable with the `concurrent-file`
    strategy. Parallelizing a pipeline that involves the R C API is
    terribly complex and eventually leads only to pseudo-parallelism
    with a lot of troubleshooting to deal with (especially to abort the
    pipeline). Consequently, we removed parallelism capabilities. The
    numerous new native metrics added in the metric engine compensate
    for that loss. The online documentation has been updated
    accordingly.

#### INTERNAL CHANGES

1.  A large number of changes to separate `lasR` from R. `lasR` can now
    be compiled as standalone software. A `Makefile` has been added to
    the repository. At the R level, the pipeline and the processing
    options are passed to the C++ engine via a JSON file instead of
    being passed via the R’s C API, effectively separating `lasR` from R
    itself. The R side of `lasR` is now purely an API to the standalone
    engine. A JSON file produced by the `lasR` package can be executed
    with the standalone software: `lasr pipeline.json`. However, the
    syntax of the JSON file is not documented and is not intended to be
    documented. Rather, the JSON file should be produced by an API such
    as the `lasR` package, a QGIS plugin, or a Python package.
    Obviously, there is currently no such thing.

## lasR 0.5.6

- Fix:
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  with COPC files, depth query (`-max_depth`), and buffer. The depth
  query was not performed at all. The fix is temporary: it breaks the
  progress bar of
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  but this is a less serious bug.
- Fix:
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  with very large files.
- Fix:
  [`load_raster()`](https://r-lidar.github.io/lasR/reference/load_raster.md)
  is thread-safe
- New:
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  accepts a new argument `default_value`. When rasterizing with an
  operator and a filter (e.g. `-keep_z_above 2`) some pixels that are
  covered by points may no longer contain any point that pass the filter
  criteria and are assigned NA. To differentiate NAs from non covered
  pixels and NAs from covered pixels but without point that pass the
  filter, the later case can be assigned another value such as 0.

## lasR 0.5.5

- Fix: [\#50](https://github.com/r-lidar/lasR/issues/50)
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  properly reprojects the bounding boxes in WGS84
- Enhance:
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  writes `zmin` and `zmax` for each file.
- Fix: [\#55](https://github.com/r-lidar/lasR/issues/55)
  [`local_maximum()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  no longer fails with `ofile = ""`
- Fix: progress bar of the
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  for COPC files.
- Fix: metrics `zsd` and `isd` were incorrect due to wrong parenthesis
  in the code.

## lasR 0.5.4

- Fix: [\#48](https://github.com/r-lidar/lasR/issues/48) segfault with
  [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md)
  when 0 points left.
- Enhance: [\#47](https://github.com/r-lidar/lasR/issues/47) pipelines
  are named `list`.
- Enhance: [\#47](https://github.com/r-lidar/lasR/issues/47) the output
  `list` returned by `exec` is named and duplicated names are made
  unique with [`make.names()`](https://rdrr.io/r/base/make.names.html)
- Doc: added some notes in the documentation of
  [`geometry_features()`](https://r-lidar.github.io/lasR/reference/geometry_features.md)
  to address question in
  [\#45](https://github.com/r-lidar/lasR/issues/45)
- Enhance: [\#49](https://github.com/r-lidar/lasR/issues/49)
  [`set_crs()`](https://r-lidar.github.io/lasR/reference/set_crs.md) no
  longer forces the pipeline to read the files.
- Enhance: [`exec()`](https://r-lidar.github.io/lasR/reference/exec.md)
  normalizes the path so users do not get an error when providing a path
  with a `~`.
- New:
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  gained a metric `zaboveX` to compute canopy cover.

## lasR 0.5.3

- Fix: [\#45](https://github.com/r-lidar/lasR/issues/45) computation
  time of `geometry_features` after
  [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md)
- Fix:
  [`local_maximum()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  was processing deleted points.
- Enhance: [\#44](https://github.com/r-lidar/lasR/issues/44) `write_vpc`
  write the `datetime`
- Enhance: `delete_points` can now physically remove the deleted points
  if the number of points deleted is important. Before they were flagged
  but kept in memory. It can also free available memory.

## lasR 0.5.2

- New: [\#42](https://github.com/r-lidar/lasR/issues/42)
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  gained an argument `absolute_path`
- Fix: [\#42](https://github.com/r-lidar/lasR/issues/42)
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  orders the long/lat coordinates properly on Linux
- Fix: [\#42](https://github.com/r-lidar/lasR/issues/42)
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  writes the absolute path of the relative path does not exist on
  Windows
- Fix: [\#40](https://github.com/r-lidar/lasR/issues/40)
  [`triangulate()`](https://r-lidar.github.io/lasR/reference/triangulate.md)
  with 0 point chunk.
- Fix: [\#43](https://github.com/r-lidar/lasR/issues/43):
  `geometry_feature` works if the file already contains some extrabytes
  attributes
- Enhance: 0 point point-clouds are no longer stopping the computation.
  If a stage such as
  [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md)
  removes all the points, the pipeline is stopped for the current
  chunk/file but the computation keep going for others. This was not the
  case for all stages and some stages could either crash or stop the
  computation.

## lasR 0.5.1

- Fix:
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  does not crash with files without CRS
- Fix:
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  write the CRS set upstream by
  [`set_crs()`](https://r-lidar.github.io/lasR/reference/set_crs.md)

## lasR 0.5.0

- New: stage
  [`geometry_features()`](https://r-lidar.github.io/lasR/reference/geometry_features.md)
  to compute point wise geometry features based on k-nearest neighbors.
- New: stage
  [`callback()`](https://r-lidar.github.io/lasR/reference/callback.md)
  can load more than 10 extrabyte attributes. Using the flag `E` all the
  extrabytes are loaded.
- New: stage
  [`set_crs()`](https://r-lidar.github.io/lasR/reference/set_crs.md) to
  assign a coordinate reference system at some point of the pipeline.
- New: raster in `GeoTiff` format are now created with
  `COMPRESS=DEFLATE`, `PREDICTOR=2`,`TILED=YES` effectively reducing the
  size of the rasters
- New: `summarize()` output includes the CRS.

## lasR 0.4.8

- Enhance: [\#33](https://github.com/r-lidar/lasR/issues/33)
  [`local_maximum()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  gained a `record_attributes` argument to chose if the attribute of the
  points are recorded in the vector file.
- Enhance: [\#33](https://github.com/r-lidar/lasR/issues/33)
  [`local_maximum_raster()`](https://r-lidar.github.io/lasR/reference/local_maximum.md)
  no longer record zeroed LAS point attributes

## lasR 0.4.7

- Fix: [\#32](https://github.com/r-lidar/lasR/issues/32) writing a
  vector file with a path containing a wildcard crashed the program.

## lasR 0.4.6

- Fix: `lax` included into `laz` file were not working.
- Fix: [\#30](https://github.com/r-lidar/lasR/issues/30) can read files
  bigger than 2.14 GB

## lasR 0.4.5

- Fix: [\#29](https://github.com/r-lidar/lasR/issues/29) using a filter
  in
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  produced corrupted output.

## lasR 0.4.4

- Fix: bug with
  `set_parallel_strategy(nested(ncores = 4, ncores2 = 4))`.
- Fix: attribute `datatime` is `datetime` in VPC files.
- Fix: [\#25](https://github.com/r-lidar/lasR/issues/25) triangulation
  with 0 points crashed. 0 points are possible with a filter.
- Fix: [\#24](https://github.com/r-lidar/lasR/issues/24)
  [`write_vpc()`](https://r-lidar.github.io/lasR/reference/write_vpc.md)
  writes the correct number of points for LAS 1.4 files.
- Fix: read of WKT strings in LAS files with a size inferior to what was
  declared in the header (null-terminated before
  `record_length_after_header`).

## lasR 0.4.3

- Fix: [\#22](https://github.com/r-lidar/lasR/issues/22) segfault with
  partial processing.
- Fix: memory access to WKT strings non-null-terminated.

## lasR 0.4.2

- Fix: `add_attribute()` was incorrectly reallocating memory causing
  potential crashes, especially when adding several attributes.
- Fix:
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md)
  crashing if the header of the LAS file did not record the correct
  number of points.
- Fix: naming of the queries.
- Documentation: reorganized the URLs and navbar of the website.

## lasR 0.4.0

- New: parallelism on multiple files. See
  [`?multithreading`](https://r-lidar.github.io/lasR/reference/multithreading.md)
- New: stage `local_maximum_raster` to compute local maximum on a raster
- New: argument `with` in `exec` to pass processing options that should
  be preferred over direct naming.
- New: function
  [`set_exec_options()`](https://r-lidar.github.io/lasR/reference/set_exec_options.md)
  to assign global processing options and override arguments potentially
  hardcoded in
  [`exec()`](https://r-lidar.github.io/lasR/reference/exec.md)
- New: stage `load_raster` to read a raster instead of producing it on
  the fly from the point cloud.
- New: stage `add_rgb` to modify the point data format
- Doc: new article on the website about parallelism for an illustrated
  version of
  [`?multithreading`](https://r-lidar.github.io/lasR/reference/multithreading.md)
- Doc: improve documentation about processing options in
  [`?exec`](https://r-lidar.github.io/lasR/reference/exec.md) and
  [`?set_exec_options`](https://r-lidar.github.io/lasR/reference/set_exec_options.md)

## lasR 0.3.6

- Fix: [\#18](https://github.com/r-lidar/lasR/issues/18) strongly
  improving arithmetic accuracy in `point_in_triangle`.

## lasR 0.3.5

- Fix: [\#17](https://github.com/r-lidar/lasR/issues/17)
  `transform_with` can be used with `pit_fill`

## lasR 0.3.4

- Fix: [\#15](https://github.com/r-lidar/lasR/issues/15) `pit_fill`
  producing corrupted output
- Fix: `pit_fill` was not respecting the parameters given by the user
- Fix: `pit_fill` in combination with `rasterize("max")` was not working
  properly

## lasR 0.3.3

- Fix: [\#12](https://github.com/r-lidar/lasR/issues/12) write lax with
  buffered chunk
- Fix: [\#13](https://github.com/r-lidar/lasR/issues/13) processing by
  chunk was not buffered

## lasR 0.3.2

- Fix: CRS are working on Windows
- Fix: [`library(lasR)`](https://github.com/r-lidar/lasR) transparently
  checks for latest version on Windows.

## lasR 0.3.1

- Fix: bugs when making a spatial query on multiple files with multiple
  spatial indexing systems (e.g. lax+nothing, lax+copc)

## lasR 0.3.0

- Change: `processor()` and
  [`reader()`](https://r-lidar.github.io/lasR/reference/reader.md) are
  deprecated and are replaced by
  [`exec()`](https://r-lidar.github.io/lasR/reference/exec.md) and
  [`reader_las()`](https://r-lidar.github.io/lasR/reference/deprecated.md).
  This intends to provide a more consistent and natural way to separate
  the pipeline. i.e the stages and the global processing options
  i.e. the buffer, the chunking, the progress bar. For example the
  following now respects the `LAScatalog` processing options and this
  was not possible with the previous syntax.

  ``` r
  ctg = lidR::readLAScatalog()
  pipeline = reader_las() + rasterize(...)
  exec(pipeline, on = ctg)
  ```

- New: the processor is now able to process by chunk like `lidR`

  ``` r
  pipeline = reader_las() + rasterize(...)
  exec(pipeline, on = file, chunk = 500)
  ```

- New: stage
  [`delete_points()`](https://r-lidar.github.io/lasR/reference/delete_points.md)
  to remove some points in the pipeline.

- New: it is now possible to write the following:

  ``` r
  dtm = dtm()
  pipeline <- read + dtm + transform_with(dtm[[2]])
  ```

- New: it is not possible to omit the reader stage. It automatically
  adds a default reader

  ``` r
  pipeline = rasterize(...)
  exec(pipeline, on = ctg)
  ```

- New: triangulation is 4x faster and uses half the memory.

- Fix: `summarize()`,
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  and [`write_las()`](https://r-lidar.github.io/lasR/reference/write.md)
  no longer process withheld points in streaming mode.

## lasR 0.2.1 (2024-03-05)

- Fix:
  [`callback()`](https://r-lidar.github.io/lasR/reference/callback.md)
  properly handles errors from the injected function
- New: handy functions `tempxyz()` to generate temp files with extension
  `.xyz`.
- New:
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  is now parallelized with internal metrics including for buffered area
  based approach
- New:
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  gained a progress bar with internal metrics.

## lasR 0.2.0 (2024-03-01)

- New:
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  gains the ability to perform a multi-resolution or buffered
  rasterization. See documentation.
- New:
  [`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
  gains numerous native metrics such as `zmax`, `zmean`, `zmedian`,
  `imax`, `imean` and so on.
- New: the internal engine gains the ability to skip the processing of
  some files of the collection and use these files only to load a
  buffer. This feature works with a `LAScatalog` from `lidR` respecting
  the `processed` attribute used in `lidR`
- Fix: loading the package being offline created a bug were R no longer
  handles errors.

## lasR 0.1.2 (2024-02-10)

- New: progress bar when reading the header of the files (`LAScatalog`)
  can be enabled with `progress = TRUE`
- Fix: progress bar starts to appear earlier i.e. from 0%. For some
  pipeline it affects the feeling of progress.

## lasR 0.1.1 (2024-02-08)

- Doc: Corrected the documentation for the argument `ncores` in
  `processor()`, which incorrectly mentioned that it was not supported.
- New: Added new functions
  [`ncores()`](https://r-lidar.github.io/lasR/reference/multithreading.md)
  and
  [`half_cores()`](https://r-lidar.github.io/lasR/reference/multithreading.md).
- Fix: Corrected the reader progress bar display when reading a las file
  with a filter and a buffer.
- Fix: Fixed the overall progress bar, which was delayed by one file and
  was showing incorrect progress.

## lasR 0.1.0 (2024-02-01)

- Open to public
- Fix: Fix the overall progress bar, which was delayed by one file and
  was showing incorrect progress.

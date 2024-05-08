# lasR ?.?.?

- Enhance: #44 `write_vpc` write the `datetime`

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

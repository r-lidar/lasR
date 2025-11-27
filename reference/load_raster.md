# Load a raster for later use

Load a raster from a disk file for later use. For example, load a DTM to
feed the
[transform_with](https://r-lidar.github.io/lasR/reference/transform_with.md)
stage or load a CHM to feed the
[pit_fill](https://r-lidar.github.io/lasR/reference/pit_fill.md) stage.
The raster is never loaded entirely. Internally, only chunks
corresponding to the currently processed point cloud are loaded. Be
careful: internally, the raster is read as float no matter the original
datatype.

## Usage

``` r
load_raster(file, band = 1L)
```

## Arguments

- file:

  character. Path to a raster file.

- band:

  integer. The band to load. It reads and loads only a single band.

## Examples

``` r
r <- system.file("extdata/bcts", "bcts_dsm_5m.tif", package = "lasR")
f <- paste0(system.file(package = "lasR"), "/extdata/bcts/")
f <- list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

# In the following pipeline, neither load_raster nor pit_fill process any points.
# The internal engine is capable of knowing that, and the LAS files won't actually be
# read. Yet the raster r will be processed by chunk following the LAS file pattern.
rr <- load_raster(r)
pipeline <- rr + pit_fill(rr)
ans <- exec(pipeline, on = f, verbose = FALSE)
```

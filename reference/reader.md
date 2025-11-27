# Initialize the pipeline

This is the first stage that must be called in each pipeline. The stage
does nothing and returns nothing if it is not associated to another
processing stage. It only initializes the pipeline. `reader()` is the
main function that dispatches into to other functions.
`reader_coverage()` processes the entire point cloud. `reader_circles()`
and `reader_rectangles()` read and process only some selected regions of
interest. If the chosen reader has no options i.e. using `reader()` it
can be omitted.

## Usage

``` r
reader(filter = "", select = "*", copc_depth = NULL, ...)

reader_coverage(filter = "", select = "*", copc_depth = NULL, ...)

reader_circles(xc, yc, r, filter = "", select = "*", copc_depth = NULL, ...)

reader_rectangles(
  xmin,
  ymin,
  xmax,
  ymax,
  filter = "",
  select = "*",
  copc_depth = NULL,
  ...
)
```

## Arguments

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

- select:

  character. Unused. Reserved for future versions.

- copc_depth:

  integer. If the files are COPC file is is possible to read the point
  hierarchy up to a given level. COPC hierarchy is 0-index. The first
  level is 0 not 1.

- ...:

  passed to other readers

- xc, yc, r:

  numeric. Circle centres and radius or radii.

- xmin, ymin, xmax, ymax:

  numeric. Coordinates of the rectangles

## Examples

``` r
f <- system.file("extdata", "Topography.las", package = "lasR")

pipeline <- reader() + rasterize(10, "zmax")
ans <- exec(pipeline, on = f)
# terra::plot(ans)

pipeline <- reader(filter = keep_z_above(1.3)) + rasterize(10, "zmean")
ans <- exec(pipeline, on = f)
# terra::plot(ans)

# read_las() with no option can be omitted
ans <- exec(rasterize(10, "zmax"), on = f)
# terra::plot(ans)

# Perform a query and apply the pipeline on a subset
pipeline = reader_circles(273500, 5274500, 20) + rasterize(2, "zmax")
ans <- exec(pipeline, on = f)
# terra::plot(ans)

# Perform a query and apply the pipeline on a subset with 1 output files per query
ofile = paste0(tempdir(), "/*_chm.tif")
pipeline = reader_circles(273500, 5274500, 20) + rasterize(2, "zmax", ofile = ofile)
ans <- exec(pipeline, on = f)
# terra::plot(ans)
```

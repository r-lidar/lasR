# Edit an attribute of the points

Edit an attribute of the points by filtering the point based on
criteria.

## Usage

``` r
edit_attribute(filter = "", attribute = "", value = 0)
```

## Arguments

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

- attribute:

  string. The name of an attribute to edit

- value:

  numeric. The value to assign. Be careful, if the user try to assign a
  value out of range of representable value for a given data type it
  will be clamped.

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.

## Examples

``` r
f <- system.file("extdata", "Example.las", package="lasR")

edit = edit_attribute(filter = c("Z < 975", "Z > 974"), attribute = "UserData", value = 2)
io = write_las(templas())
pipeline = edit + io
ans = exec(pipeline, on = f)
```

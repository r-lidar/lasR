# Local Maximum

The Local Maximum stage identifies points that are locally maximum. The
window size is fixed and circular. This stage does not modify the point
cloud. It produces a derived product in vector format. The function
`local_maximum_raster` applies on a raster instead of the point cloud

## Usage

``` r
local_maximum(
  ws,
  min_height = 2,
  filter = "",
  ofile = tempgpkg(),
  use_attribute = "Z",
  record_attributes = FALSE,
  store_in_attribute = ""
)

local_maximum_raster(
  raster,
  ws,
  min_height = 2,
  filter = "",
  ofile = tempgpkg()
)
```

## Arguments

- ws:

  numeric. Diameter of the moving window used to detect the local maxima
  in the units of the input data (usually meters).

- min_height:

  numeric. Minimum height of a local maximum. Threshold below which a
  point cannot be a local maximum. Default is 2.

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

- ofile:

  character. Full outputs are always stored on disk. If `ofile = ""`
  then the stage will not store the result on disk and will return
  nothing. It will however hold partial output results temporarily in
  memory. This is useful for stage that are only intermediate stage.

- use_attribute:

  character. Specifies the attribute to use for the operation, with "Z"
  (the coordinate) as the default. Alternatively, this can be the name
  of any other attribute, such as "Intensity", "gpstime",
  "ReturnNumber", or "HAG", if it exists. Note: The function does not
  fail if the specified attribute does not exist in the point cloud. For
  example, if "Intensity" is requested but not present, or "HAG" is
  specified but unavailable, the internal engine will return 0 for the
  missing attribute.

- record_attributes:

  The coordinates XYZ of points corresponding to the local maxima are
  recorded. It is also possible to record the attributes of theses
  points such as the intensity, return number, scan angle and so on.

- store_in_attribute:

  In addition to producing a geospatial file with the local maxima, the
  points can also be flagged: 0 if the point is not a local maximum, and
  1 if the point is a local maximum. If the attribute does not exist, it
  must first be created with
  [add_extrabytes](https://r-lidar.github.io/lasR/reference/add_attribute.md)
  (see examples).

- raster:

  LASRalgorithm. A stage that produces a raster.

## Value

This stage produces a vector. The path provided to \`ofile\` is expected
to be \`.gpkg\` or any other format supported by GDAL. Vector stages may
produce geometries with Z coordinates. Thus, it is discouraged to store
them in formats with no 3D support, such as shapefiles.

## Examples

``` r
f <- system.file("extdata", "MixedConifer.las", package = "lasR")
read <- reader()
lmf <- local_maximum(5)
ans <- exec(read + lmf, on = f)
ans
#> Simple feature collection with 177 features and 0 fields
#> Geometry type: POINT
#> Dimension:     XYZ
#> Bounding box:  xmin: 481260 ymin: 3812921 xmax: 481349.8 ymax: 3813011
#> z_range:       zmin: 2.42 zmax: 32.07
#> Projected CRS: NAD83 / UTM zone 12N
#> First 10 features:
#>                              geom
#> 1  POINT Z (481309.9 3812944 2...
#> 2   POINT Z (481294.7 3813011 16)
#> 3  POINT Z (481281.9 3813003 2...
#> 4  POINT Z (481278.4 3813002 2...
#> 5  POINT Z (481307.2 3813000 2...
#> 6  POINT Z (481265.3 3812996 1...
#> 7  POINT Z (481261.8 3812999 1...
#> 8  POINT Z (481302.9 3812929 2...
#> 9  POINT Z (481260.7 3812939 2...
#> 10 POINT Z (481262.9 3812935 1...

chm <- rasterize(1, "max")
lmf <- local_maximum_raster(chm, 5)
ans <- exec(read + chm + lmf, on = f)
# terra::plot(ans$rasterize)
# plot(ans$local_maximum, add = T, pch = 19)

# Storing LM in UserData.
lmf <- local_maximum(5, store_in_attribute = "UserData")
ans <- exec(read + lmf + write_las(), on = f)
ans
#> $local_maximum
#> Simple feature collection with 177 features and 0 fields
#> Geometry type: POINT
#> Dimension:     XYZ
#> Bounding box:  xmin: 481260 ymin: 3812921 xmax: 481349.8 ymax: 3813011
#> z_range:       zmin: 2.42 zmax: 32.07
#> Projected CRS: NAD83 / UTM zone 12N
#> First 10 features:
#>                              geom
#> 1  POINT Z (481309.9 3812944 2...
#> 2   POINT Z (481294.7 3813011 16)
#> 3  POINT Z (481281.9 3813003 2...
#> 4  POINT Z (481278.4 3813002 2...
#> 5  POINT Z (481307.2 3813000 2...
#> 6  POINT Z (481265.3 3812996 1...
#> 7  POINT Z (481302.9 3812929 2...
#> 8  POINT Z (481261.8 3812999 1...
#> 9  POINT Z (481260.7 3812939 2...
#> 10 POINT Z (481302.9 3812969 2...
#> 
#> $write_las
#> [1] "/tmp/Rtmpd9DKfI/MixedConifer.las"
#> 

# Storing in an new attribute without geospatial output
attr <- add_extrabytes("uchar", "lm", "local maximum flag")
lmf <- local_maximum(5, ofile = "", store_in_attribute = "lm")
ans <- exec(attr + lmf + write_las(), on = f)
ans
#> [1] "/tmp/Rtmpd9DKfI/MixedConifer.las"
```

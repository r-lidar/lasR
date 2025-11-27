# Sample the point cloud

Sample the point cloud, keeping one random point per pixel or per voxel
or perform a poisson sampling. This stages modify the point cloud in the
pipeline but do not produce any output. When used with a 'filter'
argument, only points that match the criteria are subsampled. Other
point are kept as is in the point cloud.

## Usage

``` r
sampling_voxel(res = 2, filter = "", ...)

sampling_pixel(
  res = 2,
  filter = "",
  method = "random",
  use_attribute = "Z",
  ...
)

sampling_poisson(distance = 2, filter = "", ...)
```

## Arguments

- res:

  numeric. pixel/voxel resolution

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

- ...:

  unused

- method:

  string can be "random" to retain one random point or "min" or "max" to
  retain the highest and lowest points respectively. For min and max
  users can use the argument \`use_attribute\` to select the highest
  intensity or highest Z, or highest gpstime or any other attributes.

- use_attribute:

  character. Specifies the attribute to use for the operation, with "Z"
  (the coordinate) as the default. Alternatively, this can be the name
  of any other attribute, such as "Intensity", "gpstime",
  "ReturnNumber", or "HAG", if it exists. Note: The function does not
  fail if the specified attribute does not exist in the point cloud. For
  example, if "Intensity" is requested but not present, or "HAG" is
  specified but unavailable, the internal engine will return 0 for the
  missing attribute.

- distance:

  numeric. Minimum distance between points for poisson sampling.

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")

read <- reader()
vox <- sampling_voxel(5) # sample 1 random points per voxel
write <- write_las()
pipeline <- read + vox + write
ans = exec(pipeline, on = f)

# Only ground points are poisson sampled. Other point are kept
vox <- sampling_poisson(10, filter = "Classification == 2")
write <- write_las()
pipeline <- read + vox + write
ans = exec(pipeline, on = f)
```

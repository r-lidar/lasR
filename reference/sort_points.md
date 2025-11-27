# Sort points in the point cloud

This stage sorts points spatially. A grid of 50 meters is applied, and
points are sorted within each cell of the grid. This increases data
locality, speeds up spatial queries, but may slightly increases the
final size of the files when compressed in LAZ format compared to the
optimal compression.

## Usage

``` r
sort_points()
```

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")
exec(sort_points(), on = f)
#> NULL
```

# Height Above Ground (HAG)

Normalize the point cloud using
[triangulate](https://r-lidar.github.io/lasR/reference/triangulate.md)
and
[transform_with](https://r-lidar.github.io/lasR/reference/transform_with.md).
This process involves triangulating the ground points and then using
`transform_with` to linearly interpolate the elevation for each point
within the corresponding triangles. The `normalize()` function modifies
the Z elevation values, effectively flattening the topography and
normalizing the point cloud based on Height Above Ground (HAG). In
contrast, the `hag()` function records the HAG in an extrabyte attribute
named 'HAG', while preserving the original Z coordinates (Height Above
Sea Level).

## Usage

``` r
normalize()

hag()
```

## See also

[triangulate](https://r-lidar.github.io/lasR/reference/triangulate.md)
[transform_with](https://r-lidar.github.io/lasR/reference/transform_with.md)

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")
pipeline <- reader() + normalize() + write_las()
exec(pipeline, on = f)
#> [1] "/tmp/RtmpXXWUgg/Topography.las"
```

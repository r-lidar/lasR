# Transform a Point Cloud Using Another Stage

This stage uses another stage to modify the point cloud in the pipeline.
When used with a Delaunay triangulation or a raster, it performs an
operation to modify the Z coordinate of the point cloud. The
interpolation method is linear in the triangle mesh and bilinear in the
raster. This can typically be used to build a normalization stage.  
When used with a 4x4 Rotation-Translation Matrix, it multiplies the
coordinates of the points to apply the rigid transformation described by
the matrix. This stage modifies the point cloud in the pipeline but does
not produce any output.

## Usage

``` r
transform_with(stage, operator = "-", store_in_attribute = "", bilinear = TRUE)
```

## Arguments

- stage:

  A stage that produces a triangulation, raster, or Rotation-Translation
  Matrix (RTM), sometimes also referred to as an "Affine Transformation
  Matrix". Can also be a 4x4 RTM matrix.

- operator:

  A string. '-' and '+' are supported (only with a triangulation or a
  raster).

- store_in_attribute:

  A string. Use an extra byte attribute to store the result (only with a
  triangulation or a raster).

- bilinear:

  bool. If the stage is a raster stage, the Z values are interpolated
  with a bilinear interpolation. FALSE to desactivate it.

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.

## RTM Matriz

**Warning:** `lasR` uses bounding boxes oriented along the XY axes of
each processed chunk to manage data location and the buffer properly.
Transforming the point cloud with a rotation matrix affects its bounding
box and how `lasR` handles the buffer. When used with a matrix that has
a rotational component, it is not safe to add stages after the
transformation unless the user is certain that there is no buffer
involved.

## See also

[triangulate](https://r-lidar.github.io/lasR/reference/triangulate.md)
[write_las](https://r-lidar.github.io/lasR/reference/write.md)

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")

# with a triangulation
mesh  <- triangulate(filter = keep_ground())
trans <- transform_with(mesh)
pipeline <- mesh + trans + write_las()
ans <- exec(pipeline, on = f)

# with a matrix
a = 20 * pi / 180
m <- matrix(c(
  cos(a), -sin(a), 0, 1000,
  sin(a), cos(a), 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1), nrow = 4, byrow = TRUE)

pipeline = transform_with(m) + write_las()
exec(pipeline, on = f)
#> $load_matrix
#>           [,1]       [,2] [,3] [,4]
#> [1,] 0.9396926 -0.3420201    0 1000
#> [2,] 0.3420201  0.9396926    0    0
#> [3,] 0.0000000  0.0000000    1    0
#> [4,] 0.0000000  0.0000000    0    1
#> 
#> $write_las
#> [1] "/tmp/RtmpedO4G2/Topography.las"
#> 
```

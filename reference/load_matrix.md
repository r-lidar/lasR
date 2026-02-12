# Load a matrix for later use

Load a matrix for later use. For example, load a matrix to feed the
[transform_with](https://r-lidar.github.io/lasR/reference/transform_with.md)
stage

## Usage

``` r
load_matrix(matrix, check = TRUE)
```

## Arguments

- matrix:

  a 4x4 matrix typically a Rotation-Translation Matrix (RTM)

- check:

  Boolean. Check matrix orthogonality.

## Examples

``` r
a = 20 * pi / 180
m <- matrix(c(
  cos(a), -sin(a), 0, 1000,
  sin(a), cos(a), 0, 0,
  0, 0, 1, 0,
  0, 0, 0, 1), nrow = 4, byrow = TRUE)

mat = load_matrix(m)
trans = transform_with(mat)
write = write_las(tempfile(fileext = ".las"))
pipeline = mat + trans + write

f <- system.file("extdata", "Topography.las", package="lasR")

exec(pipeline, on = f)
#> $load_matrix
#>           [,1]       [,2] [,3] [,4]
#> [1,] 0.9396926 -0.3420201    0 1000
#> [2,] 0.3420201  0.9396926    0    0
#> [3,] 0.0000000  0.0000000    1    0
#> [4,] 0.0000000  0.0000000    0    1
#> 
#> $write_las
#> [1] "/tmp/Rtmpd9DKfI/file24432c29ddd7.las"
#> 
```

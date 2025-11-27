# Call a user-defined function on the point cloud

Call a user-defined function on the point cloud. The function receives a
`data.frame` with the point cloud. Its first input must be the point
cloud. If the function returns anything other than a `data.frame` with
the same number of points, the output is stored and returned at the end.
However, if the output is a `data.frame` with the same number of points,
it updates the point cloud. This function can, therefore, be used to
modify the point cloud using a user-defined function. The function is
versatile but complex. A more comprehensive set of examples can be found
in the [online
tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html#callback).

## Usage

``` r
callback(fun, expose = "xyz", ..., drop_buffer = FALSE, no_las_update = FALSE)
```

## Arguments

- fun:

  function. A user-defined function that takes as first argument a
  `data.frame` with the exposed point cloud attributes (see examples).

- expose:

  character. Expose only attributes of interest to save memory (see
  details).

- ...:

  parameters of function `fun`

- drop_buffer:

  bool. If false, does not expose the point from the buffer.

- no_las_update:

  bool. If the user-defined function returns a data.frame, this is
  supposed to update the point cloud. Can be disabled.

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.

## Details

In `lasR`, the point cloud is not exposed to R in a `data.frame` like in
lidR. It is stored internally in a C++ structure and cannot be seen or
modified directly by users using R code. The `callback` function is the
only stage that allows direct interaction with the point cloud by
**copying** it temporarily into a `data.frame` to apply a user-defined
function.  
  
**expose:** the 'expose' argument specifies the data that will actually
be exposed to R. For example, 'xyzia' means that the x, y, and z
coordinates, the intensity, and the scan angle will be exposed. The
supported entries are t - gpstime, a - scan angle, i - intensity, n -
number of returns, r - return number, c - classification, u - user data,
p - point source ID, e - edge of flight line flag, R - red channel of
RGB color, G - green channel of RGB color, B - blue channel of RGB
color, N - near-infrared channel, C - scanner channel (format 6+) Also
numbers from 1 to 9 for the extra attributes data numbers 1 to 9. 'E'
enables all extra attribute to be loaded. '\*' is the wildcard that
enables everything to be exposed from the point cloud

## See also

[write_las](https://r-lidar.github.io/lasR/reference/write.md)

## Examples

``` r
f <- system.file("extdata", "Topography.las", package = "lasR")

# There is no function in lasR to read the data in R. Let's create one
read_las <- function(f)
{
  load <- function(data) { return(data) }
  read <- reader()
  call <- callback(load, expose = "xyzi", no_las_update = TRUE)
  return (exec(read + call, on = f))
}
las <- read_las(f)
head(las)
#>          X       Y        Z Intensity
#> 1 273357.1 5274360 806.5340      1340
#> 2 273357.2 5274359 806.5635       728
#> 3 273357.2 5274358 806.0248      1369
#> 4 273357.2 5274510 809.6303       589
#> 5 273357.2 5274509 809.3880      1302
#> 6 273357.2 5274508 809.4847       123

convert_intensity_in_range <- function(data, min, max)
{
  i <- data$Intensity
  i <- ((i - min(i)) / (max(i) - min(i))) * (max - min) + min
  i[i < min] <- min
  i[i > max] <- max
  data$Intensity <- as.integer(i)
  return(data)
}

read <- reader()
call <- callback(convert_intensity_in_range, expose = "i", min = 0, max = 255)
write <- write_las()
pipeline <- read + call + write
ans <- exec(pipeline, on = f)

las <- read_las(ans)
head(las)
#>          X       Y        Z Intensity
#> 1 273357.1 5274360 806.5340       137
#> 2 273357.2 5274359 806.5635        72
#> 3 273357.2 5274358 806.0248       140
#> 4 273357.2 5274510 809.6303        57
#> 5 273357.2 5274509 809.3880       133
#> 6 273357.2 5274508 809.4847         7
```

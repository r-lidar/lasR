# Digital Terrain Model

Create a Digital Terrain Model using
[triangulate](https://r-lidar.github.io/lasR/reference/triangulate.md)
and [rasterize](https://r-lidar.github.io/lasR/reference/rasterize.md).

## Usage

``` r
dtm(res = 1, add_class = NULL, ofile = temptif())
```

## Arguments

- res:

  numeric. The resolution of the raster.

- add_class:

  integer. By default it triangulates using ground and water points
  (classes 2 and 9). It is possible to provide additional classes.

- ofile:

  character. Full outputs are always stored on disk. If `ofile = ""`
  then the stage will not store the result on disk and will return
  nothing. It will however hold partial output results temporarily in
  memory. This is useful for stage that are only intermediate stage.

## See also

[triangulate](https://r-lidar.github.io/lasR/reference/triangulate.md)
[rasterize](https://r-lidar.github.io/lasR/reference/rasterize.md)

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")
pipeline <- reader() + dtm()
exec(pipeline, on = f)
#> class       : SpatRaster 
#> size        : 286, 286, 1  (nrow, ncol, nlyr)
#> resolution  : 1, 1  (x, y)
#> extent      : 273357, 273643, 5274357, 5274643  (xmin, xmax, ymin, ymax)
#> coord. ref. : NAD83(CSRS) / MTM zone 7 (EPSG:2949) 
#> source      : file21b8406b09af.tif 
#> name        : file21b8406b09af 
```

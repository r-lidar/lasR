# Digital Surface Model

Digital Surface Model using
[triangulate](https://r-lidar.github.io/lasR/reference/triangulate.md)
and [rasterize](https://r-lidar.github.io/lasR/reference/rasterize.md).
`chm()` is a alias to `dsm()` but is misleading because it actually
computes a DSM and does not include a normalization step. `chm()` is
deprecated. Technically `dsm(tin = TRUE)` is simply the association of
the stage
[`triangulate()`](https://r-lidar.github.io/lasR/reference/triangulate.md)
and
[`rasterize()`](https://r-lidar.github.io/lasR/reference/rasterize.md)
while `dsm(tin = FALSE)` is simply an alias to `rasterize("max")`.

## Usage

``` r
dsm(res = 1, tin = FALSE, ofile = tempfile(fileext = ".tif"))

chm(res = 1, tin = FALSE, ofile = tempfile(fileext = ".tif"))
```

## Arguments

- res:

  numeric. The resolution of the raster.

- tin:

  bool. By default the DSM is a point-to-raster based methods i.e. each
  pixel is assigned the elevation of the highest point. If `tin = TRUE`
  the CHM is a triangulation-based model. The first returns are
  triangulated and interpolated.

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
pipeline <- reader() + dsm()
exec(pipeline, on = f)
#> class       : SpatRaster 
#> size        : 286, 286, 1  (nrow, ncol, nlyr)
#> resolution  : 1, 1  (x, y)
#> extent      : 273357, 273643, 5274357, 5274643  (xmin, xmax, ymin, ymax)
#> coord. ref. : NAD83(CSRS) / MTM zone 7 (EPSG:2949) 
#> source      : file23d753a8131c.tif 
#> name        : max 
```

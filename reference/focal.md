# Calculate focal ("moving window") values for each cell of a raster

Calculate focal ("moving window") values for each cell of a raster using
various functions. NAs are always omitted; thus, this stage effectively
acts as an NA filler. The window is always circular. The edges are
handled by adjusting the window.

## Usage

``` r
focal(raster, size, fun = "mean", ofile = temptif())
```

## Arguments

- raster:

  LASRalgorithm. A stage that produces a raster.

- size:

  numeric. The window size \*\*in the units of the point cloud\*\*, not
  in pixels. For example, 2 means 2 meters or 2 feet, not 2 pixels.

- fun:

  string. Function to apply. Supported functions are 'mean', 'median',
  'min', 'max', 'sum'.

- ofile:

  character. Full outputs are always stored on disk. If `ofile = ""`
  then the stage will not store the result on disk and will return
  nothing. It will however hold partial output results temporarily in
  memory. This is useful for stage that are only intermediate stage.

## Value

This stage produces a raster. The path provided to \`ofile\` is expected
to be \`.tif\` or any other format supported by GDAL.

## Examples

``` r
f <- system.file("extdata", "Topography.las", package = "lasR")

chm = rasterize(2, "zmax")
chm2 = lasR:::focal(chm, 8, fun = "mean")
chm3 = lasR:::focal(chm, 8, fun = "max")
pipeline <- reader() + chm + chm2 + chm2
ans = exec(pipeline, on = f)
#> Warning: GDAL Error 1: TIFFResetField:/tmp/Rtmpd9DKfI/file244349957983.tif: Can not read TIFF directory entry.

terra::plot(ans[[1]])

terra::plot(ans[[2]])

terra::plot(ans[[3]])
```

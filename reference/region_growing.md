# Region growing

Region growing for individual tree segmentation based on Dalponte and
Coomes (2016) algorithm (see reference). Note that this stage strictly
performs segmentation, while the original method described in the
manuscript also performs pre- and post-processing tasks. Here, these
tasks are expected to be done by the user in separate functions.

## Usage

``` r
region_growing(
  raster,
  seeds,
  th_tree = 2,
  th_seed = 0.45,
  th_cr = 0.55,
  max_cr = 20,
  ofile = temptif()
)
```

## Arguments

- raster:

  LASRalgoritm. A stage producing a raster.

- seeds:

  LASRalgoritm. A stage producing points used as seeds.

- th_tree:

  numeric. Threshold below which a pixel cannot be a tree. Default is 2.

- th_seed:

  numeric. Growing threshold 1. See reference in Dalponte et al. 2016. A
  pixel is added to a region if its height is greater than the tree
  height multiplied by this value. It should be between 0 and 1. Default
  is 0.45.

- th_cr:

  numeric. Growing threshold 2. See reference in Dalponte et al. 2016. A
  pixel is added to a region if its height is greater than the current
  mean height of the region multiplied by this value. It should be
  between 0 and 1. Default is 0.55.

- max_cr:

  numeric. Maximum value of the crown diameter of a detected tree **(in
  data units)**. Default is 20. **BE CAREFUL** this algorithm exists in
  the `lidR` package and this parameter is in pixels in `lidR`.

- ofile:

  character. Full outputs are always stored on disk. If `ofile = ""`
  then the stage will not store the result on disk and will return
  nothing. It will however hold partial output results temporarily in
  memory. This is useful for stage that are only intermediate stage.

## Value

This stage produces a raster. The path provided to \`ofile\` is expected
to be \`.tif\` or any other format supported by GDAL.

## References

Dalponte, M. and Coomes, D. A. (2016), Tree-centric mapping of forest
carbon density from airborne laser scanning and hyperspectral data.
Methods Ecol Evol, 7: 1236–1245. doi:10.1111/2041-210X.12575.

## Examples

``` r
f <- system.file("extdata", "MixedConifer.las", package="lasR")

reader <- reader(filter = keep_first())
chm <- rasterize(1, "max")
lmx <- local_maximum_raster(chm, 5)
tree <- region_growing(chm, lmx, max_cr = 10)
u <- exec(reader + chm + lmx + tree, on = f)

# terra::plot(u$rasterize)
# plot(u$local_maximum, add = T, pch = 19, cex = 0.5)
# terra::plot(u$region_growing, col = rainbow(150))
# plot(u$local_maximum, add = T, pch = 19, cex = 0.5)
```

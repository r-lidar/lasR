# Digital Surface Model

The pit-free algorithm developed by Khosravipour et al. (2016), which is
based on the computation of an incremental triangulation of all returns
with triangle freezing criteria.

## Usage

``` r
spikefree(
  res = 0.5,
  freeze_distance = 1,
  height_buffer = 0.5,
  filter = "",
  ofile = temptif()
)
```

## Arguments

- res:

  resolution of the raster

- freeze_distance:

  freeze distance (see references). Recommended value: 3 times the pulse
  spacing or a little higher. Use `freeze_distance = 0` to use the
  locally adaptive spikefree by Fisher F. J. (2024) (see references)

- height_buffer:

  buffer distance (see references). Recommended value: 0.5 do not
  change.

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

- ofile:

  character. Full outputs are always stored on disk. If `ofile = ""`
  then the stage will not store the result on disk and will return
  nothing. It will however hold partial output results temporarily in
  memory. This is useful for stage that are only intermediate stage.

## References

Khosravipour, Anahita & Skidmore, Andrew & Isenburg, Martin. (2016).
Generating spike-free digital surface models using LiDAR raw point
clouds: A new approach for forestry applications. International Journal
of Applied Earth Observation and Geoinformation. 52. 104-114.
10.1016/j.jag.2016.06.005.  
  
Fischer, F. J., Jackson, T., Vincent, G., & Jucker, T. (2024). Robust
characterisation of forest structure from airborne laser scanning—A
systematic assessment and sample workflow for ecologists. Methods in
Ecology and Evolution, 15, 1873–1888.
https://doi.org/10.1111/2041-210X.14416

## Examples

``` r
f <- system.file("extdata", "Megaplot.las", package="lasR")
chm = exec(spikefree(0.1, 3), on = f)
```

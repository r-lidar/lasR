# Summary

Summarize the dataset by counting the number of points, first returns
and other metrics for the **entire point cloud**. It also produces an
histogram of Z and Intensity attributes for the **entiere point cloud**.
It can also compute some metrics **for each file or chunk** with the
same metric engine than
[rasterize](https://r-lidar.github.io/lasR/reference/rasterize.md). This
stage does not modify the point cloud. It produces a summary as a
`list`.

## Usage

``` r
summarise(zwbin = 2, iwbin = 50, metrics = NULL, filter = "")
```

## Arguments

- zwbin, iwbin:

  numeric. Width of the bins for the histograms of Z and Intensity.

- metrics:

  Character vector. "min", "max" and "count" are accepted as well as
  many others (see
  [metric_engine](https://r-lidar.github.io/lasR/reference/metric_engine.md)).
  If `NULL` nothing is computed. If something is provided these metrics
  are computed for each chunk loaded. A chunk might be a file but may
  also be a plot (see examples).

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")
read <- reader()
pipeline <- read + summarise()
ans <- exec(pipeline, on = f)
ans
#> $npoints
#> [1] 73403
#> 
#> $nsingle
#> [1] 31294
#> 
#> $nwithheld
#> [1] 0
#> 
#> $nsynthetic
#> [1] 0
#> 
#> $npoints_per_return
#>     1     2     3     4     5     6 
#> 53538 15828  3569   451    16     1 
#> 
#> $npoints_per_class
#>     1     2     9 
#> 61347  8159  3897 
#> 
#> $z_histogram
#>   788   790   792   794   796   798   800   802   804   806   808   810   812 
#>     1   163   265   470   596   694  1610  4955  5510 13833  9974  9865  8076 
#>   814   816   818   820   822   824   826   828   830 
#>  6643  4682  2958  1715   830   390   146    26     1 
#> 
#> $i_histogram
#>   50  100  150  200  250  300  350  400  450  500  550  600  650  700  750  800 
#>   25  397 1041 1576 2842 2585 2249 2051 2341 2575 2301 2529 2483 2623 2505 2697 
#>  850  900  950 1000 1050 1100 1150 1200 1250 1300 1350 1400 1450 1500 1550 1600 
#> 2759 2865 2865 3242 3272 3556 3464 3231 2589 2472 3478 3747 1969  532  183  104 
#> 1650 1700 1750 1800 1850 1900 1950 2000 2050 2100 2150 2200 2250 2300 2350 2400 
#>   97   65   35   18    9   13    7    6    0    1    0    2    1    0    0    0 
#> 2450 
#>    1 
#> 
#> $crs
#> [1] "PROJCRS[\"NAD83(CSRS) / MTM zone 7\",BASEGEOGCRS[\"NAD83(CSRS)\",DATUM[\"NAD83 Canadian Spatial Reference System\",ELLIPSOID[\"GRS 1980\",6378137,298.257222101,LENGTHUNIT[\"metre\",1]]],PRIMEM[\"Greenwich\",0,ANGLEUNIT[\"degree\",0.0174532925199433]],ID[\"EPSG\",4617]],CONVERSION[\"MTM zone 7\",METHOD[\"Transverse Mercator\",ID[\"EPSG\",9807]],PARAMETER[\"Latitude of natural origin\",0,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8801]],PARAMETER[\"Longitude of natural origin\",-70.5,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8802]],PARAMETER[\"Scale factor at natural origin\",0.9999,SCALEUNIT[\"unity\",1],ID[\"EPSG\",8805]],PARAMETER[\"False easting\",304800,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8806]],PARAMETER[\"False northing\",0,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8807]]],CS[Cartesian,2],AXIS[\"easting (E(X))\",east,ORDER[1],LENGTHUNIT[\"metre\",1]],AXIS[\"northing (N(Y))\",north,ORDER[2],LENGTHUNIT[\"metre\",1]],USAGE[SCOPE[\"Engineering survey, topographic mapping.\"],AREA[\"Canada - Quebec - between 72°W and 69°W.\"],BBOX[45.01,-72,61.8,-69]],ID[\"EPSG\",2949]]"
#> 
#> $epsg
#> [1] 2949
#> 

# Compute metrics for each plot
read = reader_circles(c(273400, 273500), c(5274450, 5274550), 11.28)
metrics = summarise(metrics = c("z_mean", "z_p95", "i_median", "count"))
pipeline = read + metrics
ans = exec(pipeline, on = f)
ans$metrics
#>   count i_median   z_mean    z_p95
#> 1   291     1311 806.0330 807.2401
#> 2   185      731 804.0168 811.2172
```

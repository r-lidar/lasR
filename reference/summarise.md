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
#> 788.000000 790.000000 792.000000 794.000000 796.000000 798.000000 800.000000 
#>         85        186        364        559        608        820       3618 
#> 802.000000 804.000000 806.000000 808.000000 810.000000 812.000000 814.000000 
#>       4885      11596      10378      10187       8937       7364       5633 
#> 816.000000 818.000000 820.000000 822.000000 824.000000 826.000000 828.000000 
#>       3766       2282       1225        588        246         64         12 
#> 
#> $i_histogram
#>   50.000000  100.000000  150.000000  200.000000  250.000000  300.000000 
#>         134         735        1265        2423        2684        2378 
#>  350.000000  400.000000  450.000000  500.000000  550.000000  600.000000 
#>        2169        2012        2604        2408        2338        2639 
#>  650.000000  700.000000  750.000000  800.000000  850.000000  900.000000 
#>        2397        2740        2507        2719        2883        2825 
#>  950.000000 1000.000000 1050.000000 1100.000000 1150.000000 1200.000000 
#>        3074        3223        3433        3576        3305        2912 
#> 1250.000000 1300.000000 1350.000000 1400.000000 1450.000000 1500.000000 
#>        2471        2935        3822        2932        1145         282 
#> 1550.000000 1600.000000 1650.000000 1700.000000 1750.000000 1800.000000 
#>         127         106          78          51          24          11 
#> 1850.000000 1900.000000 1950.000000 2000.000000 2050.000000 2100.000000 
#>          11          11           7           2           1           0 
#> 2150.000000 2200.000000 2250.000000 2300.000000 2350.000000 2400.000000 
#>           0           3           0           0           0           1 
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

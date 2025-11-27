# Filter and delete points

This stage modifies the point cloud in the pipeline but does not produce
any output. Points matching the \`filter\` criteria are
\*\*processed\*\*. In this case, it means they are deleted.
\*\*Note:\*\* In versions \< 0.17, the behavior was the opposite.

## Usage

``` r
delete_points(filter = "")

delete_noise()

delete_ground()
```

## Arguments

- filter:

  the 'filter' argument allows filtering of the point-cloud to work with
  points of interest. For a given stage when a filter is applied, only
  the points that meet the criteria are processed. The most common
  strings are `Classification == 2"`, `"Z > 2"`, `"Intensity < 100"`.
  For more details see
  [filters](https://r-lidar.github.io/lasR/reference/filters.md).

## Value

This stage transforms the point cloud in the pipeline. It consequently
returns nothing.

## Examples

``` r
f <- system.file("extdata", "Megaplot.las", package="lasR")
read <- reader()
filter <- delete_points("Z < 4") # Remove points below 4

pipeline <- read + summarise() + filter + summarise()
exec(pipeline, on = f)
#> $summary
#> $summary$npoints
#> [1] 81590
#> 
#> $summary$nsingle
#> [1] 34337
#> 
#> $summary$nwithheld
#> [1] 0
#> 
#> $summary$nsynthetic
#> [1] 0
#> 
#> $summary$npoints_per_return
#>     1     2     3     4 
#> 55756 21493  3999   342 
#> 
#> $summary$npoints_per_class
#>     1     2 
#> 74201  7389 
#> 
#> $summary$z_histogram
#>     0     2     4     6     8    10    12    14    16    18    20    22    24 
#> 11031  1256  2476  3994  4295  4898  5943  7115  8557  9934 10353  7520  3156 
#>    26    28    30 
#>   955   103     4 
#> 
#> $summary$i_histogram
#>     0    50   100   150   200   250   300   350   400   450   500   550   600 
#> 43359 37956   204    42    16     6     2     0     1     1     2     0     1 
#> 
#> $summary$crs
#> [1] "PROJCRS[\"NAD83 / UTM zone 17N\",BASEGEOGCRS[\"NAD83\",DATUM[\"North American Datum 1983\",ELLIPSOID[\"GRS 1980\",6378137,298.257222101,LENGTHUNIT[\"metre\",1]]],PRIMEM[\"Greenwich\",0,ANGLEUNIT[\"degree\",0.0174532925199433]],ID[\"EPSG\",4269]],CONVERSION[\"UTM zone 17N\",METHOD[\"Transverse Mercator\",ID[\"EPSG\",9807]],PARAMETER[\"Latitude of natural origin\",0,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8801]],PARAMETER[\"Longitude of natural origin\",-81,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8802]],PARAMETER[\"Scale factor at natural origin\",0.9996,SCALEUNIT[\"unity\",1],ID[\"EPSG\",8805]],PARAMETER[\"False easting\",500000,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8806]],PARAMETER[\"False northing\",0,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8807]]],CS[Cartesian,2],AXIS[\"(E)\",east,ORDER[1],LENGTHUNIT[\"metre\",1]],AXIS[\"(N)\",north,ORDER[2],LENGTHUNIT[\"metre\",1]],USAGE[SCOPE[\"Engineering survey, topographic mapping.\"],AREA[\"North America - between 84°W and 78°W - onshore and offshore. Canada - Nunavut; Ontario; Quebec. United States (USA) - Florida; Georgia; Kentucky; Maryland; Michigan; New York; North Carolina; Ohio; Pennsylvania; South Carolina; Tennessee; Virginia; West Virginia.\"],BBOX[23.81,-84,84,-78]],ID[\"EPSG\",26917]]"
#> 
#> $summary$epsg
#> [1] 26917
#> 
#> 
#> $summary.1
#> $summary.1$npoints
#> [1] 68328
#> 
#> $summary.1$nsingle
#> [1] 26693
#> 
#> $summary.1$nwithheld
#> [1] 0
#> 
#> $summary.1$nsynthetic
#> [1] 0
#> 
#> $summary.1$npoints_per_return
#>     1     2     3     4 
#> 47919 18297  2058    54 
#> 
#> $summary.1$npoints_per_class
#>     1 
#> 68328 
#> 
#> $summary.1$z_histogram
#>     4     6     8    10    12    14    16    18    20    22    24    26    28 
#>  1501  3994  4295  4898  5943  7115  8557  9934 10353  7520  3156   955   103 
#>    30 
#>     4 
#> 
#> $summary.1$i_histogram
#>     0    50 
#> 34738 33590 
#> 
#> $summary.1$crs
#> [1] "PROJCRS[\"NAD83 / UTM zone 17N\",BASEGEOGCRS[\"NAD83\",DATUM[\"North American Datum 1983\",ELLIPSOID[\"GRS 1980\",6378137,298.257222101,LENGTHUNIT[\"metre\",1]]],PRIMEM[\"Greenwich\",0,ANGLEUNIT[\"degree\",0.0174532925199433]],ID[\"EPSG\",4269]],CONVERSION[\"UTM zone 17N\",METHOD[\"Transverse Mercator\",ID[\"EPSG\",9807]],PARAMETER[\"Latitude of natural origin\",0,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8801]],PARAMETER[\"Longitude of natural origin\",-81,ANGLEUNIT[\"degree\",0.0174532925199433],ID[\"EPSG\",8802]],PARAMETER[\"Scale factor at natural origin\",0.9996,SCALEUNIT[\"unity\",1],ID[\"EPSG\",8805]],PARAMETER[\"False easting\",500000,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8806]],PARAMETER[\"False northing\",0,LENGTHUNIT[\"metre\",1],ID[\"EPSG\",8807]]],CS[Cartesian,2],AXIS[\"(E)\",east,ORDER[1],LENGTHUNIT[\"metre\",1]],AXIS[\"(N)\",north,ORDER[2],LENGTHUNIT[\"metre\",1]],USAGE[SCOPE[\"Engineering survey, topographic mapping.\"],AREA[\"North America - between 84°W and 78°W - onshore and offshore. Canada - Nunavut; Ontario; Quebec. United States (USA) - Florida; Georgia; Kentucky; Maryland; Michigan; New York; North Carolina; Ohio; Pennsylvania; South Carolina; Tennessee; Virginia; West Virginia.\"],BBOX[23.81,-84,84,-78]],ID[\"EPSG\",26917]]"
#> 
#> $summary.1$epsg
#> [1] 26917
#> 
#> 
```

# Point Filters

Filters are strings used in the `filter` argument of `lasR` stages to
process only the points of interest. Filters follow the format
'Attribute condition value(s)', e.g.: `Z > 2`, `Intensity < 155`,
`Classification == 2`, or `ReturnNumber == 1`.  
  
The available conditions include `>`, `<`, `>=`, `<=`, `==`, `!=`,
`%in%`, `%out%`, and `%between%`. The supported attributes are any names
of the attributes of the point cloud such as `X`, `Y`, `Z`, `Intensity`,
`gpstime`, `UserData`, `ReturnNumber`, `ScanAngle`, `Amplitude` and so
on.  
  
Valid filter strings are e.g. `Z > 2.5`, `UserData == 2`,
`Classification %in% 1 2 3`, `Z %between 1 4`. Multiple conditions can
be combined with `c("Z >= 1", "Z <= 4)`.  
  
Note that filters never fail. If a filter references an attribute not
present in the point cloud (e.g., `Intensity < 50` in a point cloud
without intensity data), the attribute is treated as if it has a value
of 0. This behavior can impact conditions like `Intensity < 50` where
all points would pass the test.  
  
For convenience, the most commonly used filters have corresponding
helper functions that return the appropriate filter string. Points that
satisfy the specified condition **are retained** for processing, while
others are ignored for the current stage.

## Usage

``` r
keep_class(x)

drop_class(x)

keep_first()

drop_first()

keep_ground()

keep_ground_and_water()

drop_ground()

keep_noise()

drop_noise()

keep_z_above(x)

drop_z_above(x)

keep_z_below(x)

drop_z_below(x)

keep_z_between(x, y)

drop_z_between(x, y)

drop_duplicates()

# S3 method for class 'laslibfilter'
print(x, ...)

# S3 method for class 'laslibfilter'
e1 + e2
```

## Arguments

- x, y:

  numeric or integer as a function of the filter used.

- ...:

  Unused.

- e1, e2:

  lasR objects.

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")
gnd = keep_class(c(2,9))
reader(gnd)
#> -----------
#> reader (uid:ea8bf0eb78c7)
#>   filter : [Classification %in% 2 9] 
#>   output :  
#> -----------
#> 
triangulate(filter = keep_ground())
#> -----------
#> triangulate (uid:44ccb0ac66a0)
#>   use_attribute : Z 
#>   max_edge : 0.00 
#>   filter : [Classification == 2] 
#>   output :  
#> -----------
#> 
rasterize(1, "max", filter = "Z > 5")
#> -----------
#> rasterize (uid:17fcc43980b0)
#>   method : [max] 
#>   window : 1.00 
#>   res : 1.00 
#>   filter : [Z > 5] 
#>   output : /tmp/Rtmpq3MByp/file240829185a88.tif 
#> -----------
#> 
```

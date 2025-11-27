# Print Information about the Point Cloud

This function prints useful information about point cloud files,
including the file version, size, bounding box, CRS, and more. When
called without parameters, it returns a pipeline stage. For convenience,
it can also be called with the path to a file for immediate execution,
which is likely the most common use case (see examples).

## Usage

``` r
info(f)
```

## Arguments

- f:

  string (optional) Path to a LAS/LAZ file.

## Value

nothing. The stage is used for its side effect of printing

## Examples

``` r
f <- system.file("extdata", "MixedConifer.las", package = "lasR")
g <- system.file("extdata", "Example.pcd", package = "lasR")

# Return a pipeline stage
exec(info(), on = f)
#> Source       : LASF (v1.2)
#> Size         : 1.47 MB
#> Extent       : 481260.00 481349.99 3812921.09 3813010.99 (xmin, xmax, ymin, ymax)
#> Points       : 37.66 thousands
#> Area         : 8090.1 m²
#> Density      : 4.7 pts/m²
#> Coord. ref.  : NAD83 / UTM zone 12N
#> Schema       :
#> 18 attributes | 39 bytes per points
#>  Name: flags             | uchar  | Desc: Internal 8-bit mask reserved for lasR core engine
#>  Name: X                 | int    | Desc: X coordinate
#>  Name: Y                 | int    | Desc: Y coordinate
#>  Name: Z                 | int    | Desc: Z coordinate
#>  Name: Intensity         | ushort | Desc: Pulse return magnitude
#>  Name: ReturnNumber      | uchar  | Desc: Pulse return number for a given output pulse
#>  Name: NumberOfReturns   | uchar  | Desc: Total number of returns for a given pulse
#>  Name: Classification    | uchar  | Desc: The 'class' attributes of a point
#>  Name: UserData          | uchar  | Desc: Used at the user’s discretion
#>  Name: PointSourceID     | short  | Desc: Source from which this point originated
#>  Name: ScanAngle         | char   | Desc: Rounded angle at which the laser point was output
#>  Name: gpstime           | double | Desc: Time tag value at which the point was observed
#>  Name: treeID            | double | Desc: An ID for each segmented tree
#>  Name: EdgeOfFlightline  | bit    | Desc: Set when the point is at the end of a scan
#>  Name: ScanDirectionFlag | bit    | Desc: Direction in which the scanner mirror was traveling 
#>  Name: Synthetic         | bit    | Desc: Point created by a technique other than direct observation
#>  Name: Keypoint          | bit    | Desc: Point is considered to be a model key-point
#>  Name: Withheld          | bit    | Desc: Point is supposed to be deleted)
#> NULL

# Convenient user-friendly usage
info(f)
#> Source       : LASF (v1.2)
#> Size         : 1.47 MB
#> Extent       : 481260.00 481349.99 3812921.09 3813010.99 (xmin, xmax, ymin, ymax)
#> Points       : 37.66 thousands
#> Area         : 8090.1 m²
#> Density      : 4.7 pts/m²
#> Coord. ref.  : NAD83 / UTM zone 12N
#> Schema       :
#> 18 attributes | 39 bytes per points
#>  Name: flags             | uchar  | Desc: Internal 8-bit mask reserved for lasR core engine
#>  Name: X                 | int    | Desc: X coordinate
#>  Name: Y                 | int    | Desc: Y coordinate
#>  Name: Z                 | int    | Desc: Z coordinate
#>  Name: Intensity         | ushort | Desc: Pulse return magnitude
#>  Name: ReturnNumber      | uchar  | Desc: Pulse return number for a given output pulse
#>  Name: NumberOfReturns   | uchar  | Desc: Total number of returns for a given pulse
#>  Name: Classification    | uchar  | Desc: The 'class' attributes of a point
#>  Name: UserData          | uchar  | Desc: Used at the user’s discretion
#>  Name: PointSourceID     | short  | Desc: Source from which this point originated
#>  Name: ScanAngle         | char   | Desc: Rounded angle at which the laser point was output
#>  Name: gpstime           | double | Desc: Time tag value at which the point was observed
#>  Name: treeID            | double | Desc: An ID for each segmented tree
#>  Name: EdgeOfFlightline  | bit    | Desc: Set when the point is at the end of a scan
#>  Name: ScanDirectionFlag | bit    | Desc: Direction in which the scanner mirror was traveling 
#>  Name: Synthetic         | bit    | Desc: Point created by a technique other than direct observation
#>  Name: Keypoint          | bit    | Desc: Point is considered to be a model key-point
#>  Name: Withheld          | bit    | Desc: Point is supposed to be deleted)

info(g)
#> Source       : PCDF (v0.7)
#> Size         : 3.75 kB
#> Extent       : 339002.88 339015.12 5248000.00 5248001.00 (xmin, xmax, ymin, ymax)
#> Points       : 30.00 
#> Area         : 12.2 m²
#> Density      : 2.4 pts/m²
#> Coord. ref.  : (null)
#> Schema       :
#> 18 attributes | 125 bytes per points
#>  Name: flags             | char   | Desc: Internal 8-bit mask reserved lasR core engine
#>  Name: X                 | float  | Desc: 
#>  Name: Y                 | float  | Desc: 
#>  Name: Z                 | float  | Desc: 
#>  Name: Intensity         | double | Desc: 
#>  Name: returnnumber      | double | Desc: 
#>  Name: NumberOfReturns   | double | Desc: 
#>  Name: ScanDirectionFlag | double | Desc: 
#>  Name: EdgeOfFlightline  | double | Desc: 
#>  Name: Classification    | double | Desc: 
#>  Name: Synthetic         | double | Desc: 
#>  Name: Keypoint          | double | Desc: 
#>  Name: withheld          | double | Desc: 
#>  Name: Overlap           | double | Desc: 
#>  Name: ScanAngle         | double | Desc: 
#>  Name: UserData          | double | Desc: 
#>  Name: PointSourceID     | double | Desc: 
#>  Name: gpstime           | double | Desc: 
```

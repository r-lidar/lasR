# Read a point cloud in memory

Read a point cloud in memory. The point cloud is stored in a C++ data
structure and is not exposed to users

## Usage

``` r
read_cloud(file, progress = TRUE)
```

## Arguments

- file:

  a file containing a point cloud. Currently only LAS and LAZ files are
  supported

- progress:

  boolean progress bar

## Examples

``` r
f <- system.file("extdata", "Topography.las", package="lasR")
las <- read_cloud(f)
#> Read files headers: [==========] 100% (1 threads)                    Overall: [          ] 0% (1 threads) | : no progress                     Overall: [          ] 0% (1 threads) | read_las: [          ] 0% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 1% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 2% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 3% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 4% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 5% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 6% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 7% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 8% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [          ] 9% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 10% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 11% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 12% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 13% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 14% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 15% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 16% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 17% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 18% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=         ] 19% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 20% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 21% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 22% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 23% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 24% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 25% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 26% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 27% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 28% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==        ] 29% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 30% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 31% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 32% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 33% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 34% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 35% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 36% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 37% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 38% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [===       ] 39% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 40% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 41% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 42% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 43% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 44% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 45% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 46% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 47% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 48% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [====      ] 49% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 50% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 51% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 52% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 53% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 54% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 55% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 56% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 57% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 58% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=====     ] 59% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 60% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 61% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 62% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 63% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 64% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 65% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 66% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 67% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 68% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [======    ] 69% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 70% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 71% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 72% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 73% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 74% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 75% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 76% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 77% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 78% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [=======   ] 79% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 80% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 81% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 82% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 83% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 84% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 85% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 86% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 87% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 88% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========  ] 89% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 90% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 91% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 92% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 93% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 94% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 95% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 96% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 97% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 98% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [========= ] 99% (1 threads)                    Overall: [          ] 0% (1 threads) | read_las: [==========] 100% (1 threads)                    Overall: [==========] 100% (1 threads) |                     Overall: [==========] 100% (1 threads)                    
las
#> Source       : LASF (v1.2)
#> Size         : 2.28 MB
#> Extent       : 273357.14 273642.86 5274357.14 5274642.85 (xmin, xmax, ymin, ymax)
#> Points       : 73.40 thousands
#> Area         : 81629.0 m²
#> Density      : 0.9 pts/m²
#> Coord. ref.  : (null)
#> Schema       :
#> 17 attributes | 31 bytes per points
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
#>  Name: EdgeOfFlightline  | bit    | Desc: Set when the point is at the end of a scan
#>  Name: ScanDirectionFlag | bit    | Desc: Direction in which the scanner mirror was traveling 
#>  Name: Synthetic         | bit    | Desc: Point created by a technique other than direct observation
#>  Name: Keypoint          | bit    | Desc: Point is considered to be a model key-point
#>  Name: Withheld          | bit    | Desc: Point is supposed to be deleted)
u = exec(chm(5), on = las)
u
#> class       : SpatRaster 
#> size        : 58, 58, 1  (nrow, ncol, nlyr)
#> resolution  : 5, 5  (x, y)
#> extent      : 273355, 273645, 5274355, 5274645  (xmin, xmax, ymin, ymax)
#> coord. ref. :  
#> source      : file210e5d82043f.tif 
#> name        : max 
```

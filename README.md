
# lasR

![license](https://img.shields.io/badge/Licence-GPL--3-blue.svg)
![Lifecycle:Experimental](https://img.shields.io/badge/Lifecycle-Experimental-339999)
[![R-CMD-check](https://github.com/r-lidar/lasR/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/r-lidar/lasR/actions/workflows/R-CMD-check.yaml)
[![Codecov test coverage](https://codecov.io/gh/r-lidar/lasR/branch/main/graph/badge.svg)](https://app.codecov.io/gh/r-lidar/lasR?branch=main)

R Package for Fast Airborne LiDAR Data Processing

**This package is in under early stage of development. Features and functions can change without warning**

The `lasR` package (pronounce laser) **does not** intent to supersede the `lidR` package, but is designed to be much more efficient than `lidR` for common tasks like the production of CHM, DTM, tree detection and segmentation on large coverages. `lidR` intends to be a tool box to make data exploration and innovation easy. `lasR` on another hand focuses exclusively on memory and speed optimization of common tasks and make no trade off with other aspects of the development.

:book: Read [the tutorial](https://r-lidar.github.io/lasR/articles/lasR2.html) to start with `lasR`

## Benchmark

The following benchmark compares how much time and RAM memory it takes for `lasR` and `lidR` to produce a DTM, a CHM, and a raster with two metrics derived from Z and intensity. The test was performed on 120 million points stored in 4 LAZ files. More details in the [benchmark](https://r-lidar.github.io/lasR/articles/lasR4.html) vignettes.

<img src="man/figures/readme_benchmark.png" style="display: block; margin: auto;" />

## Installation

There is currently no plan for releasing `lasR` on CRAN. You must compile and install the package from Github.

``` r
remotes::install_github("r-lidar/lasR")
```


## Main differences with `lidR`

- Introduces the concept of pipelines to chain multiple operations on a point cloud optimally that was missing in `lidR`
- Is written exclusively in C/C++ without a single line of R.
- Uses the code of `lidR` but brings significant speed and memory improvements.
- Does not load the point cloud into a `data.frame`. The point cloud is stored in a C++ structure that is not exposed to users.
- Uses GDAL instead of relying on `terra` and `sf` for more flexibility at the C++ level.
- Has only 2 strong dependencies: `boost` and `gdal`. But if `sf` and  `terra` are installed the experience is better.

## Copyright Information

- For `LASlib` and `LASzip`:
  - © 2007-2021 <martin.isenburg@rapidlasso.com> -
    <http://rapidlasso.com>
  - Provided under LGPL license and modified to be R-compliant by
    Jean-Romain Roussel.
- For `lasR`:
  - © 2023-2024 Jean-Romain Roussel
  - Provided under GPL-3 license.
- For `chm_prep`
  - © 2008-2023 Benoît St-Onge
  - Provided under GPL-3 license.

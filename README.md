# lasR

![license](https://img.shields.io/badge/Licence-GPL--3-blue.svg)
![Lifecycle:Maturing](https://img.shields.io/badge/Lifecycle-Maturing-339999)
[![R-CMD-check](https://github.com/r-lidar/lasR/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/r-lidar/lasR/actions/workflows/R-CMD-check.yaml)
[![Codecov test coverage](https://codecov.io/gh/r-lidar/lasR/branch/main/graph/badge.svg)](https://app.codecov.io/gh/r-lidar/lasR?branch=main)

R Package for Fast Airborne LiDAR Data Processing

The `lasR` package (pronounce laser) **does not** intent to supersede the `lidR` package, but is designed to be much more efficient than `lidR` for common tasks like the production of CHM, DTM, tree detection and segmentation on large coverages. `lidR` intends to be a tool box to make data exploration and innovation easy. `lasR` on another hand focuses on production, being optimized for memory and speed and makes no trade off with other aspects of the development.

:book: Read [the tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html) to start with `lasR`

## Installation

There is currently no plan for releasing `lasR` on CRAN. `lasR` is hosted on `r-universe` instead:

```r
install.packages('lasR', repos = 'https://r-lidar.r-universe.dev')
```

Users can't rely on the CRAN versioning system and RStudio update button to get the latest version of `lasR`. When loading `lasR` with `library(lasR)`, an internal routine checks for the latest version and prints a message if a new version is available. Updates are more frequent this way.

```r
library(lasR)
#> lasR 0.1.3 is now available. You are using 0.1.1
#> install.packages('lasR', repos = 'https://r-lidar.r-universe.dev')
```

## Benchmark

The following benchmark compares how much time and RAM memory it takes for `lasR` and `lidR` to produce a DTM, a CHM, and a raster with two metrics derived from Z and intensity. The test was performed on 120 million points stored in 4 LAZ files. More details in the [benchmark](https://r-lidar.github.io/lasR/articles/benchmarks.html) vignette.

<img src="man/figures/readme_benchmark.png" style="display: block; margin: auto;" />

## Main differences with `lidR`

- Introduces the concept of pipelines, that is missing in `lidR`, to chain multiple operations on a point cloud optimally.
- Is written exclusively in C/C++ without a single line of R.
- Uses the code of `lidR` but brings significant speed and memory improvements.
- Does not load the point cloud into a `data.frame`. The point cloud is stored in a C++ structure that is not exposed to users.
- Uses GDAL instead of relying on `terra` and `sf` for more flexibility at the C++ level.
- Has only 1 strong dependencies to `gdal`. But if `sf` and  `terra` are installed the experience is better.

More details in the corresponding [vignette](https://r-lidar.github.io/lasR/articles/lasR1.html#main-differences-between-lasr-and-lidr)

## About

`lasR` is developed at [Laval University](https://www.ulaval.ca/en).

<img src="https://upload.wikimedia.org/wikipedia/fr/b/bf/Universit%C3%A9_Laval_logo_et_texte.svg" width="300">

## Copyright Information

- For `lasR`:
  - © 2023-2024 Jean-Romain Roussel
  - Provided under GPL-3 license.
- For `LASlib` and `LASzip`:
  - © 2007-2021 Martin Isenburg - <http://rapidlasso.com>
  - Provided under LGPL license and modified to be R-compliant by
    Jean-Romain Roussel.
- For `chm_prep`
  - © 2008-2023 Benoît St-Onge - [Geophoton-inc/chm_prep](https://github.com/Geophoton-inc/chm_prep)
  - Provided under GPL-3 license.
- For `json` parser:
  - Lohmann, N. (2023). JSON for Modern C++ (Version 3.11.3) [Computer software]. https://github.com/nlohmann
  - Provided under MIT license
- For `delaunator`:
  - © 2018 Volodymyr Bilonenko. [delfrrr/delaunator-cpp](https://github.com/delfrrr/delaunator-cpp)
  - Provided under MIT license

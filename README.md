lasR <img src="https://github.com/r-lidar/lasR/blob/main/man/figures/lasR200x231.png?raw=true" align="right"/>
======================================================================================================

![license](https://img.shields.io/badge/Licence-GPL--3-blue.svg)
![Lifecycle:Maturing](https://img.shields.io/badge/Lifecycle-Maturing-339999)
[![R-CMD-check](https://github.com/r-lidar/lasR/actions/workflows/R-CMD-check.yaml/badge.svg)](https://github.com/r-lidar/lasR/actions/workflows/R-CMD-check.yaml)
[![Codecov test coverage](https://codecov.io/gh/r-lidar/lasR/branch/main/graph/badge.svg)](https://app.codecov.io/gh/r-lidar/lasR?branch=main)

R Package for Fast Airborne LiDAR Data Processing

The `lasR` package (pronounced "laser") is an extremely fast software capable of creating and applying complex processing pipelines on terabytes of airborne lidar data. It can read and write `.las` and `.laz` files, compute metrics using an area-based approach, compute digital canopy models, segment individual trees, thin point data, process a collection of tiles using multicore processing, and provides other tools to process terabytes of lidar data in a **production context**.

- 📖 Read [the tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html) to start with `lasR`
- 💻 Install `lasR` from R with: `install.packages('lasR', repos = 'https://r-lidar.r-universe.dev')`
- 💵 [Sponsor `lasR`](https://github.com/sponsors/Jean-Romain)

`lasR` **does not** intent to supersede the [`lidR`](https://github.com/r-lidar/lidR) package, but is designed to be much more efficient than `lidR` for common tasks like the production of CHM, DTM, tree detection and segmentation on large coverages (see [details](https://r-lidar.github.io/lasR/articles/why.html#main-differences-between-lasr-and-lidr)).

## Installation

There is currently no plan for releasing `lasR` on CRAN. `lasR` is hosted on `r-universe`:

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
- Has more powerfull algorithm
- Is written exclusively in C/C++ without a single line of R. The R code is only an API to a standalone C++ software.
- Does not load the point cloud into a `data.frame`. The point cloud is stored in a C++ structure that is not exposed to users.
- Has only 1 strong dependencies to `gdal`. But if `sf` and  `terra` are installed the experience is better.
- `lidR` intends to be a tool box to make data exploration and innovation easy. `lasR` on another hand focuses on production, being optimized for memory and speed and makes no trade off with other aspects of the development.

More details in the corresponding [vignette](https://r-lidar.github.io/lasR/articles/why.html#main-differences-between-lasr-and-lidr)

## About

`lasR` is developed openly by [r-lidar](https://www.r-lidar.com/).

The development of the `lidR` package was made possible in the past thanks to the financial support of the [Laval University](https://www.ulaval.ca/en).

## Copyright Information

- For `lasR`:
  - © 2023-2024 Jean-Romain Roussel
  - Licence: GPL-3
- For `LASlib` and `LASzip`:
  - © 2007-2021 Martin Isenburg - <http://rapidlasso.com>
  - Licence: LGPL  (modified to be R-compliant by Jean-Romain Roussel)
- For `chm_prep`:
  - © 2008-2023 Benoît St-Onge - [Geophoton-inc/chm_prep](https://github.com/Geophoton-inc/chm_prep)
  - Licence: GPL-3
- For `json` parser:
  - Lohmann, N. (2023). JSON for Modern C++ (Version 3.11.3) [Computer software]. https://github.com/nlohmann
  - Licence: MIT
- For `delaunator`:
  - © 2018 Volodymyr Bilonenko. [delfrrr/delaunator-cpp](https://github.com/delfrrr/delaunator-cpp)
  - Licence: MIT
- For `Eigen`:
  - Guennebaud, Gaël and Jacob, Benoît and others
  - Eigen: A C++ linear algebra library http://eigen.tuxfamily.org
  - Licence: MPL2
- For `Cloth Simulation Filter (CSF)`
  - © 2017 State Key Laboratory of Remote Sensing Science, Institute of Remote Sensing Science and Engineering, Beijing Normal University
  - Licence: Apache
  - W. Zhang, J. Qi, P. Wan, H. Wang, D. Xie, X. Wang, and G. Yan, “An Easy-to-Use Airborne LiDAR Data Filtering Method Based on Cloth Simulation,” Remote Sens., vol. 8, no. 6, p. 501, 2016.

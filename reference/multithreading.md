# Parallel processing tools

`lasR` uses OpenMP to paralellize the internal C++ code.
`set_parallel_strategy()` globally changes the strategy used to process
the point clouds. `sequential()`, `concurrent_files()`,
`concurrent_points()`, and `nested()` are functions to assign a
parallelization strategy (see Details). `has_omp_support()` tells you if
the `lasR` package was compiled with the support of OpenMP which is
unlikely to be the case on MacOS.

## Usage

``` r
set_parallel_strategy(strategy)

unset_parallel_strategy()

get_parallel_strategy()

ncores()

half_cores()

sequential()

concurrent_files(ncores = half_cores())

concurrent_points(ncores = half_cores())

nested(ncores = ncores()/4L, ncores2 = 2L)

has_omp_support()
```

## Arguments

- strategy:

  An object returned by one of `sequential()`, `concurrent_points()`,
  `concurrent_files()` or `nested()`.

- ncores:

  integer. Number of cores.

- ncores2:

  integer. Number of cores. For `nested` strategy `ncores` is the number
  of concurrent files and `ncores2` is the number of concurrent points.

## Details

There are 4 strategies of parallel processing:

- sequential:

  No parallelization at all: `sequential()`

- concurrent-points:

  Point cloud files are processed sequentially one by one. Inside the
  pipeline, some stages are parallelized and are able to process
  multiple points simultaneously. Not all stages are natively
  parallelized. E.g. `concurrent_points(4)`

- concurrent-files:

  Files are processed in parallel. Several files are loaded in memory
  and processed simultaneously. The entire pipeline is parallelized, but
  inside each stage, the points are processed sequentially. E.g.
  `concurrent_files(4)`

- nested:

  Files are processed in parallel. Several files are loaded in memory
  and processed simultaneously, and inside some stages, the points are
  processed in parallel. E.g. `nested(4,2)`

`concurrent-files` is likely the most desirable and fastest option.
However, it uses more memory because it loads multiple files. The
default is `concurrent_points(half_cores())` and can be changed globally
using e.g. `set_parallel_strategy(concurrent_files(4))`

## Examples

``` r
if (FALSE) { # \dontrun{
f <- paste0(system.file(package="lasR"), "/extdata/bcts/")
f <- list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

pipeline <- reader_las() + rasterize(2, "imean")

ans <- exec(pipeline, on = f, progress = TRUE, ncores = concurrent_files(4))

set_parallel_strategy(concurrent_files(4))
ans <- exec(pipeline, on = f, progress = TRUE)
} # }
```

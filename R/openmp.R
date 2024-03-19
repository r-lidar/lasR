#' Parallel processing tools
#'
#' `lasR` uses OpenMP to paralellize the internal C++ code. `set_parallel_strategy()` globally changes
#' the strategy used to process the point clouds. `sequential()`, `concurrent_files()`,
#' `concurrent_points()`, and `nested()` are functions to assign a parallelization strategy (see Details).
#' `has_omp_support()` tells you if the `lasR` package was compiled with the support of OpenMP which
#' is unlikely to be the case on MacOS.
#'
#' There are 4 strategies of parallel processing:
#' \describe{
#' \item{sequential}{No parallelization at all: `sequential()`}
#' \item{concurrent-points}{Point cloud files are processed sequentially one by one. Inside the pipeline,
#' some stages are parallelized and are able to process multiple points simultaneously. Not all stages
#' are natively parallelized. E.g. `concurrent_points(4)`}
#' \item{concurrent-files}{Files are processed in parallel. Several files are loaded in memory
#' and processed simultaneously. The entire pipeline is parallelized, but inside each stage,
#' the points are processed sequentially. E.g. `concurrent_files(4)`}
#' \item{nested}{Files are processed in parallel. Several files are loaded in memory
#' and processed simultaneously, and inside some stages, the points are processed in parallel. E.g. `nested(4,2)`}
#' }
#' `concurrent-files` is likely the most desirable and fastest option. However, it uses more memory
#' because it loads multiple files. The default is `concurrent_points(half_cores())` and can be changed
#' globally using e.g. `set_parallel_strategy(concurrent_files(4))`

#'
#' @param strategy An object returned by one of `sequential()`, `concurrent_points()`, `concurrent_files` or
#' `nested()`.
#' @param ncores integer. Number of cores.
#' @param ncores2 integer.  Number of cores. For `nested` strategy `ncores` is the number of concurrent
#' files and `ncores2` is the number of concurrent points.
#'
#' @md
#' @rdname multithreading
#' @name multithreading
#' @examples
#' \dontrun{
#' f <- paste0(system.file(package="lasR"), "/extdata/bcts/")
#' f <- list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
#'
#' pipeline <- reader_las() + rasterize(2, "imean")
#'
#' ans <- exec(pipeline, on = f, progress = TRUE, ncores = concurrent_files(4))
#'
#' set_parallel_strategy(concurrent_files(4))
#' ans <- exec(pipeline, on = f, progress = TRUE)
#' }
NULL

#' @rdname multithreading
#' @export
set_parallel_strategy <- function(strategy)
{
  ncores <- as.integer(strategy)
  modes <- c("sequential", "concurrent-points", "concurrent-files", "nested")
  mode <- attr(strategy, "strategy")
  if (is.null(mode)) strategy = "concurrent-points"
  mode <- match.arg(mode, modes)
  mode <- match(mode, modes)
  LASROPTIONS$ncores <- ncores
  LASROPTIONS$strategy <- mode
}

#' @rdname multithreading
#' @export
unset_parallel_strategy <- function()
{
  LASROPTIONS$ncores <- NULL
  LASROPTIONS$strategy <- NULL
}

#' @rdname multithreading
#' @export
get_parallel_strategy = function()
{
  modes <- c("sequential", "concurrent-points", "concurrent-files", "nested")
  ncores <- LASROPTIONS$ncores
  if (is.null(ncores)) return(NULL)
  attr(ncores, "strategy") <- modes[LASROPTIONS$strategy]
  return(ncores)
}

#' @rdname multithreading
#' @export
ncores <- function() { available_threads() }

#' @rdname multithreading
#' @export
half_cores <- function(){ as.integer(ncores()/2L) }

#' @rdname multithreading
#' @export
sequential <- function()
{
  ncores <- 1L
  attr(ncores, "strategy") <- "sequential"
  return(ncores)
}

#' @rdname multithreading
#' @export
concurrent_files <- function(ncores = half_cores())
{
  if (ncores > ncores()) ncores <- ncores()
  attr(ncores, "strategy") <- "concurrent-files"
  return(ncores)
}

#' @rdname multithreading
#' @export
concurrent_points <- function(ncores = half_cores())
{
  if (ncores > ncores()) ncores <- ncores()
  attr(ncores, "strategy") <- "concurrent-points"
  return(ncores)
}

#' @rdname multithreading
#' @export
nested <- function(ncores = ncores()/4L, ncores2 = 2L)
{
  ncores = c(ncores, ncores2)
  attr(ncores, "strategy") <- "nested"
  return(ncores)
}

#' @rdname multithreading
#' @export
has_omp_support = function() { .Call(`C_has_omp_support`) }

available_threads <- function() { .Call(`C_available_threads`) }

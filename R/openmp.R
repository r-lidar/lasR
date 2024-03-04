#' Parallel processing tools
#'
#' `set_lasr_strategy()` globally changes the strategy used to process the point-clouds. `ncores()`
#' returns the number of available CPU cores that can be utilized. `half_cores()`
#' returns half this number. `sequential()`, `concurrent_files()` and `concurrent_points()`
#' are functions to assign a parallelization strategy to \link{processor} (see Details)
#'
#' There are 4 strategies of parallel processing:
#' \describe{
#' \item{sequential}{No parallelization at all: `sequential()`}
#' \item{concurent-points}{Point cloud files are process sequentially one by one. Inside the pipeline
#' some stages are parallelized and are able to process multiple points simultaneously. Not all stages
#' are natively parallelized: `concurrent_points(4)` }
#' \item{concurent-files}{Files are process in parallel. Several files are loaded in memory
#' and processed simultaneously. The entire pipeline is parallelized but inside each stage
#' the points are process sequentially: `concurrent_files(4)`}
#' \item{nested}{Files are process in parallel. Several files are loaded in memory
#' and processed simultaneously and inside some stages the points are process in parallel: `nested(4,2)` }
#' }
#' `concurrent-files` is likely the most desirable and fastest option on modern computers with
#' fast drive and many cores. However it uses more memory because it loads multiples files. The default
#' is `concurrent-points` and can be changed globally using e.g. `set_lasr_strategy(concurrent_files(4))`
#'
#' @param strategy An object return by one of `sequential()`, `concurrent_points()`, `concurrent_files` or
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
#' read <- reader(f)
#' met <- rasterize(2, "imean")
#' pipeline <- read + met
#'
#' set_lasr_strategy(concurrent_files(4))
#' ans <- processor(pipeline, progress = TRUE)
#' }
NULL

#' @rdname multithreading
#' @export
set_lasr_strategy <- function(strategy)
{
  ncores <- as.integer(strategy)
  modes <- c("sequential", "concurrent-points", "concurrent-files", "nested")
  mode <- attr(strategy, "strategy")
  if (is.null(mode)) strategy = concurrent_points()
  mode <- match.arg(mode, modes)
  mode <- match(mode, modes)
  LASRTHREADS$ncores <- ncores
  LASRTHREADS$strategy <- mode
}

#' @rdname multithreading
#' @export
get_lasr_strategy = function()
{
  modes <- c("sequential", "concurrent-points", "concurrent-files", "nested")
  ncores <- LASRTHREADS$ncores
  attr(ncores, "strategy") <- modes[LASRTHREADS$strategy]
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


LASRTHREADS = new.env()
LASRTHREADS$ncores = 1L
LASRTHREADS$strategy <- 1L


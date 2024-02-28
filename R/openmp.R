#' Parallel processing tools
#'
#' `ncores()` returns the number of available CPU cores that can be utilized. `half_cores()`
#' returns half this number. `sequential()`, `concurrent_files()` and `concurrent_points()`
#' are functions to assign a parallelization strategy in \link{processor}.
#'
#' @param ncores integer. Number of cores.
#' @return An integer representing the number of available CPU cores or the number of cores to use.
#' `sequential()`, `concurrent_files()` and `concurrent_points()` return a number with an attribute
#' `strategy`.
#'
#' @export
#' @md
#' @rdname multithreading
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


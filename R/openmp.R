#' Determine the number of available cores
#'
#' `ncores()` returns the number of available CPU cores that can be utilized. `half_cores()`
#' returns half this number. `sequential()`, `concurrent_files()` and `concurrent_points()`
#' are convenient functions to assign a parallelization mode in \link{processor}.
#'
#' @return An integer representing the number of available CPU cores.
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
sequential <- function() { return("sequential") }

#' @rdname multithreading
#' @export
concurrent_files <- function() { return("concurrent-files") }

#' @rdname multithreading
#' @export
concurrent_points <- function() { return("concurrent-points") }


#' Determine the number of available cores
#'
#' This function returns the number of available CPU cores that can be utilized.
#'
#' @return An integer representing the number of available CPU cores.
#'
#' @export
ncores <- function() { available_threads() }

#' @rdname ncores
#' @export
half_cores <- function(){ as.integer(ncores()/2L) }

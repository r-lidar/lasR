#' Point filters
#'
#' lasR uses LASlib/LASzip, the library developed by Martin Isenburg to read and write LAS/LAZ files.
#' Thus, the flags that are available in `LAStools` are also available in `lasR`.
#' Filters are strings to put in the `filter` arguments of the `lasR` algorithms. The list of available
#' strings is accessible with `filter_usage`. For convenience, the most useful filters have an associated
#' function that returns the corresponding string.
#'
#' @param x numeric or integer as a function of the filter used.
#' @param e1,e2 lasR objects.
#' @param ... Unused.
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' filter_usage()
#' gnd = keep_class(c(2,9))
#' reader_las(gnd)
#' triangulate(filter = keep_ground())
#' rasterize(1, "max", filter = "-drop_z_below 5")
#' @name filters
#' @rdname filters
#' @md
NULL

#' @rdname filters
#' @export
keep_class = function(x) { make_filter(paste("-keep_class", paste(x, collapse = " "))) }

#' @rdname filters
#' @export
drop_class = function(x) { make_filter(paste("-drop_class", paste(x, collapse = " "))) }

#' @rdname filters
#' @export
keep_first = function() { make_filter("-keep_first")}

#' @rdname filters
#' @export
drop_first = function() { make_filter("-drop_first")}

#' @rdname filters
#' @export
keep_ground = function() { keep_class(2L) }

#' @rdname filters
#' @export
drop_ground = function() { drop_class(2L) }

#' @rdname filters
#' @export
keep_noise = function() { keep_class(18L) }

#' @rdname filters
#' @export
drop_noise = function() { drop_class(18L) }

#' @rdname filters
#' @export
keep_z_above = function(x) { make_filter(paste("-keep_z_above", x[1]))}

#' @rdname filters
#' @export
drop_z_above = function(x) { make_filter(paste("-drop_z_above", x[1]))}

#' @rdname filters
#' @export
keep_z_below = function(x) { make_filter(paste("-keep_z_below", x[1]))}

#' @rdname filters
#' @export
drop_z_below = function(x) { make_filter(paste("-drop_z_below", x[1]))}

#' @rdname filters
#' @export
drop_duplicates = function() { make_filter("-drop_duplicates")}


#' @rdname filters
#' @export
filter_usage <- function() { invisible(.Call(`C_lasfilterusage`)) }

transform_usage <- function() { invisible(.Call(`C_lastransformusage`)) }

make_filter = function(x)
{
  class(x) <- "laslibfilter"
  x
}

#' @rdname filters
#' @export
print.laslibfilter = function(x, ...)
{
  class(x) <- "character"
  print(x)
}

#' @rdname filters
#' @export
`+.laslibfilter` <- function(e1, e2)
{
  if (!methods::is(e2, "character"))
    e2 <- make_filter(e2)

  if (!methods::is(e2, "laslibfilter"))
    stop("Both operands must be of class laslibfilter")

  ans <- paste(e1, e2)
  ans <- make_filter(ans)
  return(ans)
}

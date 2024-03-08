#' Temporary files
#'
#' Convenient functions to create temporary file with a given extension.
#'
#' @return string. Path to a temporary file.
#' @name temporary_files
#' @rdname temporary_files
#' @examples
#' tempshp()
#' templaz()
NULL

#' @rdname temporary_files
#' @export
temptif = function(){ tempfile_with_ext("tif") }

#' @rdname temporary_files
#' @export
tempgpkg = function() { tempfile_with_ext("gpkg") }

#' @rdname temporary_files
#' @export
tempshp = function() { tempfile_with_ext("shp") }

#' @rdname temporary_files
#' @export
templas = function() { tempfile_with_ext("las") }

#' @rdname temporary_files
#' @export
templaz = function() { tempfile_with_ext("laz") }

tempfile_with_ext = function(ext) { tempfile(fileext = paste0(".", ext)) }

#' Read a point cloud in memory
#'
#' Read a point cloud in memory. The point cloud is stored in a C++ data structure
#' and is not exposed to users
#'
#' @param file a file containing a point cloud. Currently only LAS and LAZ files are supported
#' @param progress boolean progress bar
#' @export
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' las <- read_cloud(f)
#' las
#' u = exec(chm(5), on = las)
#' u
read_cloud = function(file, progress = TRUE)
{
  stopifnot(all(file.exists(file)), length(file) == 1, is.character(file))
  file = normalizePath(file)
  xptr <- list(algoname = "xptr")
  xptr = set_lasr_class(xptr)
  ans = exec(xptr, on = file, noread = T, progress = progress)
  ans = list(ans)
  class(ans) = "lasrcloud"
  return(ans)
}

#' Print Method for 'lasrcloud' Objects
#'
#' This function defines a custom print method for objects of class 'lasrcloud'.
#'
#' @param x An object of class 'lasrcloud'.
#' @param ... Additional arguments (not used).
#' @export
#' @method print lasrcloud
print.lasrcloud <- function(x, ...)
{
  pipeline = info()
  exec(pipeline, on = x, noread = T)
}

#' Plot Method for 'lasrcloud' Objects
#'
#' This function defines a custom plot method for objects of class 'lasrcloud'.
#'
#' @param x An object of class 'lasrcloud'.
#' @param ... Additional arguments (not used).
# @export
#' @method plot lasrcloud
#' @examples
#' \dontrun{
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' las <- read_cloud(f)
#' las
#' plot(las)
#' }
#' @noRd
plot.lasrcloud <- function(x, ...)
{
  #total = .Call(`C_plot_pointcloud`, x[[1]])
}
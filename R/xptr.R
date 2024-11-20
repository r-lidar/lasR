read_las = function(file)
{
  stopifnot(all(file.exists(file)), length(file) == 1, is.character(file))
  file = normalizePath(file)
  xptr <- list(algoname = "xptr")
  xptr = set_lasr_class(xptr)
  ans = exec(xptr, on = f, noread = T)
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
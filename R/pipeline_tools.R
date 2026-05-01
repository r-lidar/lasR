#' Tools inherited from base R
#'
#' Tools inherited from base R
#'
#' @param x,e1,e2 lasR objects
#' @param i index
#' @param ... lasR objects. Is equivalent to +
#' @examples
#' algo1 <- rasterize(1, "max")
#' algo2 <- rasterize(4, "min")
#' print(algo1)
#' pipeline <- algo1 + algo2
#' print(pipeline)
#' @name tools
#' @rdname tools
NULL

#' @rdname tools
#' @export
print.PipelinePtr <- function(x, ...)
{
  .APIOPERATIONS$print_pipeline(x)
}

#' @export
#' @method print PipelinePtr
print.PipelinePtr <- function(x, ...)
{
  .APIOPERATIONS$print_pipeline(x)
}

#' @rdname tools
#' @export
`+.PipelinePtr` <- function(e1, e2)
{
  if (!methods::is(e1, "PipelinePtr") || !methods::is(e2, "PipelinePtr"))
    stop("Both operands must be of class PipelinePtr") # nocov

  ans = .APIOPERATIONS$merge_pipeline(e1, e2);

  # Carry over SEXPs that callback/aggregate stages reference by raw address
  # (func/args/call/env). The C++ pipeline stores their addresses in the JSON
  # config, so the underlying R objects must outlive every '+' to avoid GC
  # reusing the slot.
  preserved <- c(attr(e1, "preserved"), attr(e2, "preserved"))
  for (key in c("func", "args", "call", "env"))
  {
    v1 <- attr(e1, key); if (!is.null(v1)) preserved <- c(preserved, list(v1))
    v2 <- attr(e2, key); if (!is.null(v2)) preserved <- c(preserved, list(v2))
  }
  if (length(preserved) > 0) attr(ans, "preserved") <- preserved

  return(ans)
}

#' @rdname tools
#' @export
`[[.PipelinePtr` <- function(x, i, ...) {
  # Example: Access an element by name or index
  if (is.character(i)) {
    stop("Extraction by name not supported")
  } else if (is.numeric(i)) {
    return(.APIOPERATIONS$get_stage_by_index(x, i-1))
  } else {
    stop("Index must be character or numeric")
  }
}

get_pipeline_info = function(pipeline)
{
  .APIOPERATIONS$get_pipeline_info(pipeline)
}

is_indexed = function(files)
{
  ans = logical(length(files))
  for (i in seq_along(files))
  {
    file = files[i]
    indexed = .APIUTILS$is_indexed(file)
    ans[i] = indexed
  }

  return (ans)
}

address = function(x)
{
  .APIOPERATIONS$get_address(x)
}


#' Deprecated
#'
#' Deprecated function replaced by \link{exec} and \link{reader_las}
#'
#' @param pipeline a LASRpipeline. A serie of algorithms called in order
#' @param ncores integer. Number of cores to use. Some stages or some steps in some stages
#' are parallelised but overall one file is process at a time.
#' @param progress boolean. Displays a progress bar.
#' @param x Can be the paths of the files to use, the path of the folder in which the files are stored,
#' the path to a [virtual point cloud](https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/)
#' file or a `data.frame` containing hte point cloud. It supports also a `LAScatalog` or a `LAS` objects
#' from `lidR`.
#' @template param-filter
#' @param buffer numeric. Each file is read with a buffer. The default is 0, which does not mean that
#' the file won't be buffered. It means that the internal routine knows if a buffer is needed and will
#' pick the greatest value between the internal suggestion and this provided value.
#' @param xc,yc,r numeric. Circle centres and radius or radii.
#' @param xmin,ymin,xmax,ymax numeric. Coordinates of the rectangles
#' @param ... passed to other readers
#' @name deprecated
NULL

# nocov start

#' @rdname deprecated
#' @export
processor = function(pipeline, ncores = half_cores(), progress = FALSE, ...)
{
  dots <- list(...)
  verbose <- if (is.null(dots$verbose)) FALSE else TRUE
  noread <- if (is.null(dots$noread)) FALSE else TRUE

  if (!is.null(dots$ncores))
  {
    warning("Argument ncores is deprecated. Use 'set_lasr_strategy()' instead")
    set_parallel_strategy(concurrent_points(dots$ncores))
  }
  else
  {
    ncores <- LASROPTIONS$ncores
    strategy <- LASROPTIONS$strategy
  }


  args = list(progress = progress,
              ncores = ncores,
              verbose = verbose,
              strategy = strategy,
              chunk = 0)

  ans <- .Call(`C_process`, pipeline, args)

  if (inherits(ans, "error"))
  {
    stop(ans)
  }

  if (!noread) ans <- read_as_common_r_object(ans)
  ans <- Filter(Negate(is.null), ans)

  if (length(ans) == 0)
    return(NULL)
  else if (length(ans) == 1L)
    return(ans[[1]])
  else
    return(ans)
}

#' @rdname deprecated
#' @export
reader = function(x, filter = "", buffer = 0, ...)
{
  p <- list(...)
  circle <- !is.null(p$xc)
  rectangle <-!is.null(p$xmin)

  if (circle) return(reader_circles(x, p$xc, p$yc, p$r, filter = filter, buffer = buffer, ...))
  if (rectangle) return(reader_rectangles(x, p$xmin, p$ymin, p$xmax, p$ymax, filter = filter, buffer = buffer, ...))
  return(reader_coverage(x, filter = filter, buffer = buffer, ...))
}

#' @rdname deprecated
#' @export
reader_coverage = function(x, filter = "", buffer = 0, ...)
{
  if (methods::is(x, "LAScatalog") | is.character(x)) return(reader_files_coverage(x, filter, buffer, ...))
  if (methods::is(x, "LAS") | is.data.frame(x)) return(reader_dataframe_coverage(x, filter, buffer, ...))
  stop("'x' must be a character vector, a data.frame or a LAS* object from lidR.") # nocov
}

#' @rdname deprecated
#' @export
reader_circles = function(x, xc, yc, r, filter = "", buffer = 0, ...)
{
  if (methods::is(x, "LAScatalog") | is.character(x)) return(reader_files_circles(x, xc, yc, r, filter, buffer, ...))
  if (methods::is(x, "LAS") | is.data.frame(x)) return(reader_dataframe_circles(x, xc, yc, r, filter, buffer, ...))
  stop("'x' must be a character vector, a data.frame or a LAS* object from lidR.") # nocov
}

#' @rdname deprecated
#' @export
reader_rectangles = function(x, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
{
  if (methods::is(x, "LAScatalog") | is.character(x)) return(reader_files_rectangles(x, xmin, ymin, xmax, ymax, filter, buffer, ...))
  if (methods::is(x, "LAS") | is.data.frame(x)) return(reader_dataframe_rectangles(x, xmin, ymin, xmax, ymax, filter, buffer, ...))
  stop("'x' must be a character vector, a data.frame or a LAS* object from lidR.") # nocov
}

reader_files_coverage = function(files, filter = "", buffer = 0, ...)
{
  p <- list(...)
  noprocess <- p$noprocess
  chunk_size = 0

  if (methods::is(files, "LAScatalog"))
  {
    chunk_size <- files@chunk_options$size
    chunk_buffer <- files@chunk_options$buffer
    chunk_alignment <- files@chunk_options$alignment
    progress_bar <- files@processing_options$progress

    processed <- files$processed
    if (!is.null(processed)) noprocess <- !processed

    files <- files$filename

    if (chunk_buffer > buffer) buffer <- chunk_buffer
  }

  if (!is.character(files))
  {
    stop("'files' must be character or a LAScatalog")  # nocov
  }

  if (!is.null(noprocess))
  {
    if (length(noprocess) != length(files))
      stop("'noprocess' and 'files' have different length")
  }

  files <- normalizePath(files)
  ans <- list(algoname = "reader_las", files = files, filter = filter, buffer = buffer, noprocess = noprocess)
  set_lasr_class(ans)
}

reader_files_circles = function(files, xc, yc, r, filter = "", buffer = 0, ...)
{
  stopifnot(length(xc) == length(yc))
  if (length(r) == 1) r <- rep(r, length(xc))
  if (length(r) > 1) stopifnot(length(xc) == length(r))

  ans <- reader_files_coverage(files, filter, buffer, ...)
  ans[[1]]$xcenter <- xc
  ans[[1]]$ycenter <- yc
  ans[[1]]$radius <- r
  ans
}

reader_files_rectangles = function(files, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
{
  stopifnot(length(xmin) == length(ymin), length(xmin) == length(xmax), length(xmin) == length(ymax))

  ans <- reader_files_coverage(files, filter, buffer)
  ans[[1]]$xmin <- xmin
  ans[[1]]$ymin <- ymin
  ans[[1]]$xmax <- xmax
  ans[[1]]$ymax <- ymax
  ans
}

reader_dataframe_coverage = function(dataframe, filter = "", buffer = 0, ...)
{
  accuracy = c(0,0,0)
  if (methods::is(dataframe, "LAS"))
  {
    accuracy <- c(dataframe@header@PHB[["X scale factor"]], dataframe@header@PHB[["Y scale factor"]], dataframe@header@PHB[["Z scale factor"]])
    crs <- dataframe@crs$wkt
    dataframe <- dataframe@data
    attr(dataframe, "accuracy") <- accuracy
    attr(dataframe, "crs") <- crs
  }

  stopifnot(is.data.frame(dataframe))

  crs <- attr(dataframe, "crs")
  if (is.null(crs)) crs = ""
  if (!is.character(crs) & length(crs != 1)) stop("The CRS of this data.frame is not a WKT string")

  acc <- attr(dataframe, "accuracy")
  if (is.null(acc)) acc = c(0, 0, 0)
  if (!is.numeric(acc) & length(acc) != 3L) stop("The accuracy of this data.frame is not valid")

  ans <- list(algoname = "reader_dataframe", dataframe = dataframe, accuracy = acc, crs = crs, filter = filter, buffer = buffer)
  set_lasr_class(ans)
}

reader_dataframe_circles = function(dataframe, xc, yc, r, filter = "", buffer = 0, ...)
{
  stopifnot(length(xc) == length(yc))
  if (length(r) == 1) r <- rep(r, length(xc))
  if (length(r) > 1) stopifnot(length(xc) == length(r))

  ans <- reader_dataframe_coverage(dataframe, filter, buffer)
  ans[[1]]$xcenter <- xc
  ans[[1]]$ycenter <- yc
  ans[[1]]$radius <- r
  ans
}

reader_dataframe_rectangles = function(dataframe, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
{
  stopifnot(length(xmin) == length(ymin), length(xmin) == length(xmax), length(xmin) == length(ymax))

  ans <- reader_dataframe_coverage(dataframe, filter, buffer)
  ans[[1]]$xmin <- xmin
  ans[[1]]$ymin <- ymin
  ans[[1]]$xmax <- xmax
  ans[[1]]$ymax <- ymax
  ans
}

# nocov end
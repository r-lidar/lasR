#' Process the pipeline
#'
#' Process the pipeline. Every other functions of the package do nothing. This function must be
#' called on a pipeline in order to actually process the point-cloud. To process in parallel using
#' multiple cores, refer to the \link{multithreading} page.
#'
#' @param pipeline a pipeline. A serie of stages called in order
#' @param on Can be the paths of the files to use, the path of the folder in which the files are stored,
#' the path to a [virtual point cloud](https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/)
#' file or a `data.frame` containing tte point cloud. It supports also a `LAScatalog` or a `LAS` objects
#' from `lidR`.
#' @param buffer numeric. Each file is read with a buffer. The default is 0, which does not mean that
#' the file won't be buffered. It means that the internal routine knows if a buffer is needed and will
#' pick the greatest value between the internal suggestion and this value. If `on` is a `LAScatalog`
#' from `lidR` the options set by the `LAScatalog` has the precedence.
#' @param progress boolean. Displays a progress bar.  If `on` is a `LAScatalog` from `lidR` the
#' options set by the `LAScatalog` has the precedence.
#' @param chunk numeric. By default the collection of files is processed by file (`chunk = 0`). It is
#' possible to process by arbitrary sized chunks. This is useful for e.g. processing collection with
#' large files or processing a massive `copc` files. If `on` is a `LAScatalog` from `lidR` the
#' option set by the `LAScatalog` has the precedence.
#' @param ... unused
#'
#' @seealso [multithreading]
#' @examples
#' \dontrun{
#' f <- paste0(system.file(package="lasR"), "/extdata/bcts/")
#' f <- list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
#'
#' read <- reader_las()
#' tri <- triangulate(15)
#' dtm <- rasterize(5, tri)
#' lmf <- local_maximum(5)
#' met <- rasterize(2, "imean")
#' pipeline <- read + tri + dtm + lmf + met
#' set_lasr_strategy(concurrent_files(4))
#' ans <- exec(pipeline, f)
#' }
#' @md
#' @export
exec = function(pipeline, on, progress = FALSE, buffer = 0, chunk = 0, ...)
{
  if (pipeline[[1]]$algoname != "reader_las")
  {
    pipeline = reader_las() + pipeline
  }

  # use processor() so exec is backward compatible with previous pipeline format
  if (missing(on))
  {
    reader = pipeline[[1]]
    if (!is.null(reader$files) || !is.null(reader$dataframe))
    {
      return(processor(pipeline, ncores = ncores, progress = progress, ...))
    }
  }

  dots <- list(...)
  noprocess <- dots$noprocess
  verbose <- isTRUE(dots$verbose)
  noread <- isTRUE(dots$noread)
  ncores <- LASRTHREADS$ncores
  strategy <- LASRTHREADS$strategy

  valid = FALSE

  if (methods::is(on, "LAS") || is.data.frame(on))
  {
    accuracy = c(0,0,0)

    if (methods::is(on, "LAS"))
    {
      accuracy <- c(on@header@PHB[["X scale factor"]], on@header@PHB[["Y scale factor"]], on@header@PHB[["Z scale factor"]])
      crs <- on@crs$wkt
      on <- on@data
      attr(on, "accuracy") <- accuracy
      attr(on, "crs") <- crs
    }

    crs <- attr(on, "crs")
    if (is.null(crs)) crs = ""
    if (!is.character(crs) & length(crs != 1)) stop("The CRS of this data.frame is not a WKT string")

    acc <- attr(on, "accuracy")
    if (is.null(acc)) acc = c(0, 0, 0)
    if (!is.numeric(acc) & length(acc) != 3L) stop("The accuracy of this data.frame is not valid")

    pipeline[[1]]$algoname = "reader_dataframe"
    pipeline[[1]]$dataframe = on
    pipeline[[1]]$accuracy = acc
    pipeline[[1]]$buffer = buffer
    pipeline[[1]]$crs = crs

    valid = TRUE
  }

  if (methods::is(on, "LAScatalog"))
  {
    chunk <- on@chunk_options$size
    buffer <- on@chunk_options$buffer
    alignment <- on@chunk_options$alignment
    progress <- on@processing_options$progress

    processed <- on$processed
    if (!is.null(processed)) noprocess <- !processed

    on <- on$filename
  }

  if (is.character(on))
  {
    pipeline[[1]]$files <- on
    pipeline[[1]]$buffer <- buffer
    pipeline[[1]]$noprocess <- noprocess

    if (!is.null(noprocess))
    {
      if (length(noprocess) != length(on))
        stop("'noprocess' and 'on' have different length")
    }

    valid = TRUE
  }

  if (!valid) stop("Invalid argument 'on'.")


  args = list(progress = progress,
              ncores = ncores,
              verbose = verbose,
              chunk_size = chunk,
              strategy = strategy)

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

read_as_common_r_object = function(ans)
{
  nothing <- function(x) { return(x) }
  rreader <- if (system.file(package='terra') != "") terra::rast else nothing
  vreader <- if (system.file(package='sf') != "") sf::read_sf else nothing
  preader <- nothing

  ans <- lapply(ans, function(x)
  {
    if (is.character(x))
    {
      pos <- regexpr("\\.([[:alnum:]]+)$", x)
      ext <- ifelse(pos > -1L, substring(x, pos + 1L), "")
      if (length(ext) == 1)
      {
        if (ext %in% c("tif", "tiff", "jpg", "jpeg", "png", "gif", "bmp", "jp2", "ecw", "sid", "kml", "kmz", "gdb", "vrt", "dxf"))
        {
          return(rreader(x))
        }
        else if (ext %in% c("bna", "csv", "e00", "fgb", "gdb", "geojson", "gml", "gmt", "gpkg", "gps", "gpx", "gtm", "gxt", "jml", "kml", "map", "mdb", "nc", "ods", "osm", "pbf", "shp", "sqlite", "vdv", "xls", "xlsx"))
        {
          return(vreader(x))
        }
        else if (ext %in% c("las", "laz"))
        {
          return(preader(x))
        }
        else
        {
          return(x)
        }
      }
      else
      {
        return(x)
      }
    }
    else
    {
      return(x)
    }
  })

  return(ans)
}

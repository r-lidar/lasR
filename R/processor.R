#' Process the pipeline
#'
#' Process the pipeline. Every other functions of the package do nothing. This function must be
#' called on a pipeline in order to actually process the point-cloud.
#'
#' @section Multithreading:
#' There are 4 strategies of parallel processing:
#' \describe{
#' \item{sequential}{No parallelization at all: \link{sequential}}
#' \item{concurent-points}{Point cloud files are process sequentially one by one. Inside the pipeline
#' some stages are parallelized and are able to process multiple points simultaneously. Not all stages
#' are natively parallelized: \link{concurrent_points}}
#' \item{concurent-files}{Files are process in parallel. Several files are loaded in memory
#' and processed simultaneously. The entire pipeline is parallelized but inside each stage
#' the points are process sequentially: \link{concurrent_files}}
#' \item{nested}{**Not yet supported**. Files are process in parallel. Several files are loaded in memory
#' and processed simultaneously and inside some stages the points are process in parallel.}
#' }
#' `concurrent-files` is likely the most desirable and fastest option on modern computers with
#' fast drive and many cores. However it uses more memory because it loads multiples files.
#' Also some stages do not support this type a parallelism because they call R code and R
#' is not multithreaded. For example a pipeline that implies the \link{callback()} stage
#' does not support `concurrent-files` multithreading because some R code is involved. The default
#' is `concurrent-points` and can be changed globally using
#' `options("lasR.default_parallel_strategy") = concurrent_files(4)`
#'
#' @param pipeline a LASRpipeline. A serie of stages called in order (see examples)
#' @param ncores integer. Number of cores to use. Use one of \link{sequential} \link{concurrent_points}
#' \link{conccurent_files} rather than a raw integer to select a parallelization strategy (see section Multithreading).
#' If a raw number is provided the default is `concurrent-points`
#' @param progress boolean. Displays a progress bar.
#' @param ... unused
#'
#' @examples
#' \dontrun{
#' f <- paste0(system.file(package="lasR"), "/extdata/bcts/")
#' f <- list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
#'
#' read <- reader(f, filter = "")
#' tri <- triangulate(15)
#' dtm <- rasterize(5, tri)
#' lmf <- local_maximum(5)
#' met <- rasterize(2, "imean")
#' pipeline <- read + tri + dtm + lmf + met
#' ans <- processor(pipeline, ncores = concurrent_files(4), progress = T)
#' }
#' @seealso [ncores()] [half_cores()] [sequential()] [concurrent_points()] [concurrent_files()]
#'
#' @export
#' @md
processor = function(pipeline, ncores = getOption("lasR.default_parallel_strategy"), progress = FALSE, ...)
{
  dots <- list(...)
  verbose <- isTRUE(dots$verbose)
  noread <- isTRUE(dots$noread)

  modes <- c("sequential", "concurrent-points", "concurrent-files")
  mode <- attr(ncores, "strategy")
  if (is.null(mode)) ncores = concurrent_points(ncores)
  mode <- match.arg(mode, modes)
  mode <- match(mode, modes)

  ans <- .Call(`C_process`, pipeline, progress, ncores, mode, verbose)

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

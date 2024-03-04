#' Process the pipeline
#'
#' Process the pipeline. Every other functions of the package do nothing. This function must be
#' called on a pipeline in order to actually process the point-cloud. To process in parallel using
#' multiple cores, refer to the \link{multithreading} page.
#'
#' @param pipeline a LASRpipeline. A serie of stages called in order (see examples)
#' @param progress boolean. Displays a progress bar.
#' @param ... unused
#'
#' @seealso [multithreading]
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
#'
#' set_lasr_strategy(concurrent_files(4))
#' ans <- processor(pipeline, progress = T)
#' }
#' @export
#' @md
processor = function(pipeline, progress = FALSE, ...)
{
  dots <- list(...)
  verbose <- isTRUE(dots$verbose)
  noread <- isTRUE(dots$noread)

  if (!is.null(dots$ncores))
  {
    warning("Argument ncores is deprecated. Use 'set_lasr_strategy()' instead")
    set_lasr_strategy(concurrent_points(dots$ncores))
  }
  else
  {
    ncores <- LASRTHREADS$ncores
    strategy <- LASRTHREADS$strategy
  }

  ans <- .Call(`C_process`, pipeline, progress, ncores, strategy, verbose)

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

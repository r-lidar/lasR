#' Process the pipeline
#'
#' Process the pipeline. Every other functions do nothing. This function must be called on a pipeline
#' to actually process the point-cloud
#'
#' There are 4 modes of parallelization:
#' \describe{
#' \item{Sequential}{No parallelization at all: `ncores = 1`, `n_concurrent_files = 1`}
#' \item{Concurent points}{Files are process sequentially one by one and inside the pipeline
#' some parts/stages are parallelized and are able to process multiple points simultaneously.
#' `ncores = n`, `n_concurrent_files = 1`}
#' \item{Concurent files}{Files are process in parallel. Several files are loaded in memory
#' and processed simultaneously. The entire pipeline is parallelized but inside each stage
#' the points are process sequentially. `ncores = 1`, `n_concurrent_files = n`}
#' \item{Mixed}{Files are process in parallel. Several files are loaded in memory
#' and processed simultaneously. The entire pipeline is parallelized and inside each stage
#' the points are process in parallel `ncores = n`, `n_concurrent_files = m`}
#' }
#' `Mixed` is reserved for experts. `Concurent file` is likely the most desirable option
#' on modern laptop with fast drive and many cores. However it uses more memory because it
#' loads multiples files. Also some stages do not support this type a parallelism because
#' they call R code and R is not multi-threaded. For example a stage that implies \link{callback()}
#' does not support concurrent files multi-threading because some R code is involved.
#'
#' @param pipeline a LASRpipeline. A serie of stages called in order
#' @param ncores,n_concurrent_files integer. Number of cores to use. See details.
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
#' met <- rasterize(2, mean(Intensity))
#' pipeline <- read + tri + dtm + lmf + met
#' ans <- processor(pipeline)
#' }
#' @export
#' @md
processor = function(pipeline, ncores = 1L, n_concurrent_files = 2L, progress = FALSE, ...)
{
  dots <- list(...)
  verbose <- isTRUE(dots$verbose)
  noread <- isTRUE(dots$noread)

  ans <- .Call(`C_process`, pipeline, progress, ncores, n_concurrent_files, verbose)

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
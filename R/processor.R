#' Process the pipeline
#'
#' Process the pipeline. Every other functions of the package do nothing. This function must be
#' called on a pipeline in order to actually process the point-cloud. To process in parallel using
#' multiple cores, refer to the \link{multithreading} page.
#'
#' @param pipeline a pipeline. A serie of stages called in order
#' @param on Can be the paths of the files to use, the path of the folder in which the files are stored,
#' the path to a [virtual point cloud](https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/)
#' file or a `data.frame` containing the point cloud. It supports also a `LAScatalog` or a `LAS` objects
#' from `lidR`.
#' @param with list. A list of options to control how the pipeline is executed. This includes options to
#' control parallel processing, progress bar display, tile buffering and so on. See \link{set_exec_options}
#' for more details on the available options.
#' @param ... The processing options can be explicitly named and passed outside the `with` argument.
#' See \link{set_exec_options}
#'
#' @seealso [multithreading] [set_exec_options]
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
#' ans <- exec(pipeline, on = f, with = list(progress = TRUE))
#' }
#' @md
#' @export
exec = function(pipeline, on, with = NULL, ...)
{
  args = list(...)
  fjson = args$json
  async_com = ""

  # Parse options and give precedence to 1. global options 2. LAScatalog 3. with arguments 4. ... arguments
  with = parse_options(on, with, ...)

  if (is.character(pipeline))
  {
    if (!file.exists(pipeline))
      stop("File does not exist")

    file_content <- readLines(pipeline, warn = FALSE)
    processed_content <- lapply(file_content, interpolate_R_expression)
    processed_content <- do.call(c, processed_content)
    json_file <- tempfile(fileext = ".json")
    writeLines(processed_content, json_file)
    async_com = tempfile(fileext = ".tmp")
  }
  else if (methods::is(pipeline, "LASRpipeline"))
  {
    # A reader stage is mandatory. It is allowed to omit it, the engine adds it.
    if (is_reader_missing(pipeline))
    {
      pipeline = reader_las() + pipeline
    }

    # A build_catalog stage is automatically added. Users do not need to take care of that one
    pipeline = build_catalog(on, with) + pipeline

    on_is_valid = FALSE

    # If 'on' is a LAS from lidR or a data.frame. Compatibility mode for R exclusively. The reader_las
    # stage is modified to call R specific stages that are not compiled outside of R
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

      pipeline[[1]]$dataframe = on
      pipeline[[1]]$crs = crs
      pipeline[[1]]$files = NULL
      pipeline[[1]]$type = "dataframe"

      ind = get_reader_index(pipeline)
      pipeline[[ind]]$algoname = "reader_dataframe"
      pipeline[[ind]]$dataframe = on
      pipeline[[ind]]$accuracy = acc
      pipeline[[ind]]$crs = crs

      on_is_valid = TRUE
    }

    # If 'on' is a LAScatalog, for convenient compatibility with lidR we pick-up the file names
    if (methods::is(on, "LAScatalog"))
    {
      on <- on$filename
    }

    if (methods::is(on, "lasrcloud"))
    {
      pipeline[[1]]$files = NULL
      pipeline[[1]]$externalptr = on[[1]]
      pipeline[[1]]$type = "externalptr"

      ind = get_reader_index(pipeline)
      pipeline[[ind]]$algoname = "reader_externalptr"
      pipeline[[ind]]$externalptr = on[[1]]

      on_is_valid = TRUE
    }

    # If 'on' is character, this is the default behavior.
    if (is.character(on))
    {
      pipeline$build_catalog$files <- normalizePath(on, mustWork = FALSE)
      pipeline$build_catalog$buffer <- with$buffer
      pipeline$build_catalog$noprocess <- with$noprocess
      pipeline$build_catalog$type = "files"

      # 'noprocess' is a hidden and not documented options to be compatible with LAScatalog$processed
      if (!is.null(with$noprocess))
      {
        if (length(with$noprocess) != length(on))
          stop("'noprocess' and 'on' have different length")
      }

      on_is_valid = TRUE
    }

    if (!on_is_valid) stop("Invalid argument 'on'.")

    # The pipeline is a 'list' and is serialized in a JSON file. The path to the JSON file is
    # sent to the processor
    pipeline = list(processing = with, pipeline = pipeline)
    json_file = write_json(pipeline)
    if (!is.null(fjson)) file.copy(json_file, normalizePath(fjson, mustWork = FALSE))
  }

  ans <- .Call(`C_process`, json_file, async_com)

  #file.remove(json_file)

  if (inherits(ans, "error")) stop(ans)

  if (!with$noread) ans <- read_as_common_r_object(ans)
  ans <- Filter(Negate(is.null), ans)
  names(ans) <- make.names(names(ans), unique = TRUE)

  if (length(ans) == 0)
    return(NULL)
  else if (length(ans) == 1L)
    return(ans[[1]])
  else
    return(ans)
}

is_reader_missing = function(pipeline)
{
  for (stage in pipeline)
  {
    if (stage$algoname == "reader_las")
    {
      return(FALSE)
    }
  }
  return(TRUE)
}

get_reader_index = function(pipeline)
{
  i = 1
  for (stage in pipeline)
  {
    if (stage$algoname == "reader_las")
    {
      return(i)
    }

    i = i+1
  }

  return(i)
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
          return(vreader(x, as_tibble = FALSE))
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

parse_options = function(on, with, ...)
{
  dots <- list(...)

  # Default
  buffer <- 0
  chunk <- 0
  progress <- FALSE
  ncores <- if (has_omp_support()) concurrent_points(half_cores()) else sequential()
  noprocess <- NULL
  verbose <- FALSE
  noread <- FALSE
  profiling <- ""

  # Explicit options
  if (!is.null(dots$buffer)) buffer <- dots$buffer
  if (!is.null(dots$chunk))  chunk <- dots$chunk
  if (!is.null(dots$progress)) progress <- dots$progress
  if (!is.null(dots$ncores)) ncores <- dots$ncores
  if (!is.null(dots$noprocess)) noprocess <- dots$noprocess
  if (!is.null(dots$verbose)) verbose <- dots$verbose
  if (!is.null(dots$noread)) noread <- dots$noread
  if (!is.null(dots$profiling)) profiling <- dots$profiling

  # 'with' list has precedence
  if (!is.null(with$buffer)) buffer <- with$buffer
  if (!is.null(with$chunk))  chunk <- with$chunk
  if (!is.null(with$progress)) progress <- with$progress
  if (!is.null(with$ncores))  ncores <- with$ncores
  if (!is.null(with$noprocess)) noprocess <- with$noprocess
  if (!is.null(with$verbose)) verbose <- with$verbose
  if (!is.null(with$noread)) noread <- with$noread
  if (!is.null(with$profiling)) profiling <- with$profiling

  if (!missing(on))
  {
    # If 'on' is a LAScatalog it has the precedence on the 'with' list
    if (methods::is(on, "LAScatalog"))
    {
      chunk <- on@chunk_options$size
      buffer <- on@chunk_options$buffer
      alignment <- on@chunk_options$alignment
      progress <- on@processing_options$progress
      processed <- on$processed
      if (!is.null(processed)) noprocess <- !processed
      if (on@input_options$filter != "") warning(paste0("This LAScatalog has filter = \"", on@input_options$filter, "\" but this option is not automatically propagated to the 'reader_las()' stage.") , call. = FALSE, immediate. = TRUE)
    }
  }

  strategy <- ncores
  ncores <- as.integer(strategy)
  modes <- c("sequential", "concurrent-points", "concurrent-files", "nested")
  mode <- attr(strategy, "strategy")
  if (is.null(mode)) mode = "concurrent-files"
  mode <- match.arg(mode, modes)

  # Global options have precedence on everything
  if (!is.null(LASROPTIONS$buffer)) buffer <- LASROPTIONS$buffer
  if (!is.null(LASROPTIONS$chunk))  chunk <- LASROPTIONS$chunk
  if (!is.null(LASROPTIONS$progress)) progress <- LASROPTIONS$progress
  if (!is.null(LASROPTIONS$ncores)) ncores <- LASROPTIONS$ncores
  if (!is.null(LASROPTIONS$strategy)) mode <- LASROPTIONS$strategy
  if (!is.null(LASROPTIONS$verbose)) verbose <- LASROPTIONS$verbose
  if (!is.null(LASROPTIONS$noread)) noread <- LASROPTIONS$noread

  if (!has_omp_support())
  {
    if (ncores[1] > 1) warning("This version of lasR has no OpenMP support")
    ncores <- 1L
    mode <- "sequential"
  }

  stopifnot(is.numeric(buffer))
  stopifnot(is.numeric(chunk))
  stopifnot(is.logical(progress))
  stopifnot(is.numeric(ncores))
  stopifnot(is.character(mode))
  stopifnot(is.logical(verbose))
  stopifnot(is.logical(noread))

  ret = list(ncores = ncores,
             strategy = mode,
             buffer = buffer,
             progress = progress,
             noprocess = noprocess,
             chunk = chunk,
             noread = noread,
             verbose = verbose,
             profiling = profiling)

  return(ret)
}

#' Set global processing options
#'
#' Set global processing options for the \link{exec} function. By default, pipelines are executed
#' without a progress bar, processing one file at a time sequentially. The following options can be
#' passed to the `exec()` function in four ways. See details.
#'
#' There are 4 ways to pass processing options, and it is important to understand the precedence rules:\cr\cr
#' The first option is by explicitly naming each option. This option is deprecated and used for convenience and
#' backward compatibility.
#' \preformatted{
#' exec(pipeline, on = f, progress = TRUE, ncores = 8)
#' }
#' The second option is by passing a `list` to the `with` argument. This option is more explicit
#' and should be preferred. The `with` argument takes precedence over the explicit arguments.
#' \preformatted{
#' exec(pipeline, on = f, with = list(progress = TRUE, chunk = 500))
#' }
#' The third option is by using a `LAScatalog` from the `lidR` package. A `LAScatalog` already carries
#' some processing options that are respected by the `lasR` package. The options from a `LAScatalog`
#' take precedence.
#' \preformatted{
#' exec(pipeline, on = ctg, ncores = 4)
#' }
#' The last option is by setting global processing options. This has global precedence and is mainly intended
#' to provide a way for users to override options if they do not have access to the `exec()` function.
#' This may happen when a developer creates a function that executes a pipeline internally, and users cannot
#' provide any options.
#' \preformatted{
#' set_exec_options(progress = TRUE, ncores = concurrent_files(2))
#' exec(pipeline, on = f)
#' }
#' @param ncores An object returned by one of `sequential()`, `concurrent_points()`, `concurrent_files()`, or
#' `nested()`. See \link{multithreading}. If `NULL` the default is `concurrent_points(half_cores())`. If
#' a simple integer is provided it corresponds to `concurrent_files(ncores)`.
#' @param buffer numeric. Each file is read with a buffer. The default is NULL, which does not mean that
#' the file won't be buffered. It means that the internal routine knows if a buffer is needed and will
#' pick the greatest value between the internal suggestion and this value.
#' @param progress boolean. Displays a progress bar.
#' @param chunk numeric. By default, the collection of files is processed by file (`chunk = NULL` or `chunk = 0`).
#' It is possible to process in arbitrary-sized chunks. This is useful for e.g., processing collections
#' with large files or processing a massive `copc` file.
#' @param ... Other internal options not exposed to users.
#' @seealso [multithreading]
#' @export
#' @md
set_exec_options = function(ncores = NULL, progress = NULL, buffer = NULL, chunk = NULL, ...)
{
  if (!is.null(ncores)) stopifnot(is.numeric(ncores))
  if (!is.null(progress)) stopifnot(is.logical(progress))
  if (!is.null(buffer)) stopifnot(is.numeric(buffer))
  if (!is.null(chunk)) stopifnot(is.numeric(chunk))

  set_parallel_strategy(ncores)

  dots <- list(...)
  LASROPTIONS$progress <- progress
  LASROPTIONS$chunk <- chunk
  LASROPTIONS$buffer <- buffer
  LASROPTIONS$noread <- dots$noread
  LASROPTIONS$noprocess <- dots$noprocess
  LASROPTIONS$verbose <- dots$verbose
}

#' @export
#' @rdname set_exec_options
unset_exec_option = function()
{
  LASROPTIONS$progress <- NULL
  LASROPTIONS$chunk <- NULL
  LASROPTIONS$buffer <- NULL
  LASROPTIONS$noread <- NULL
  LASROPTIONS$noprocess <- NULL
  LASROPTIONS$verbose <- NULL
}

write_json = function(config)
{
  # convert R object (rasterize, callback) into pointer addresses for JSON serialization
  for (i in seq_along(config$pipeline))
  {
    stage <- config$pipeline[[i]]

    if (stage$algoname == "callback")
    {
      stage$fun <- address(stage$fun)
      stage$args <- address(stage$args)
    }
    else if (stage$algoname == "aggregate")
    {
      stage$call <- address(stage$call)
      stage$env <- address(stage$env)
    }
    else if (stage$algoname == "reader_dataframe" || stage$algoname == "build_catalog" || stage$algoname == "reader_externalptr")
    {
      if (!is.null(stage$dataframe))
        stage$dataframe = address(stage$dataframe)

      if (!is.null(stage$externalptr))
        stage$externalptr = address(stage$externalptr)
    }

    config$pipeline[[i]] <- stage
  }

  config$pipeline = unname(config$pipeline)

  json = tempfile(fileext = ".json")
  config = toJSON(config)
  write(config, json)

  return(json)
}



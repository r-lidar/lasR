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
  else if (methods::is(pipeline, "PipelinePtr"))
  {
    on_is_valid = FALSE

    # If 'on' is a LAS from lidR or a data.frame. Compatibility mode for R exclusively. The reader_las
    # stage is modified to call R specific stages that are not compiled outside of R
    if (methods::is(on, "LAS") || is.data.frame(on))
    {
      stop("Not supported yet in the new api")

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
    }

    # If 'on' is a LAScatalog, for convenient compatibility with lidR we pick-up the file names
    if (methods::is(on, "LAScatalog"))
    {
      on <- on$filename
    }

    if (methods::is(on, "lasrcloud"))
    {
      stop("Not supported yet in the new api")
    }

    # If 'on' is character, this is the default behavior.
    if (is.character(on))
    {
      stopifnot(length(on) > 0)

      #pipeline$build_catalog$files <- normalizePath(on, mustWork = FALSE)
      #pipeline$build_catalog$buffer <- with$buffer
      #pipeline$build_catalog$noprocess <- with$noprocess
      #pipeline$build_catalog$type = "files"

      on_is_valid = TRUE
    }

    if (!on_is_valid) stop("Invalid argument 'on'.")
  }

  ans <- .APIOPERATIONS$excecute_pipeline(pipeline, on, with$buffer, with$chunk, with$ncores, with$verbose, with$progress)

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
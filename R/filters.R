#' Point Filters
#'
#' Filters are strings used in the `filter` argument of `lasR` stages to process only the points of
#' interest. Filters follow the format 'Attribute condition value(s)', e.g.: `Z > 2`, `Intensity < 155`,
#' `Classification == 2`, or `ReturnNumber == 1`. \cr
#' The available conditions include `>`, `<`, `>=`, `<=`, `==`, `!=`, `%in%`, `%out%`, and `%between%`.
#' The supported attributes are any names of the attributes of the point cloud such as `X`, `Y`, `Z`, `Intensity`,
#' `gpstime`, `UserData`, `ReturnNumber`, `ScanAngle`, `Amplitude` and so on.\cr
#' Note that filters never fail. If a filter references an attribute not present in the point cloud
#' (e.g., `Intensity < 50` in a point cloud without intensity data), the attribute is treated as if it
#' has a value of 0. This behavior can impact conditions like `Intensity < 50`.\cr
#' For convenience, the most commonly used filters have corresponding helper functions that return
#' the appropriate filter string. Points that satisfy the specified condition **are retained** for
#' processing, while others are ignored for the current stage.
#'
#' @param x numeric or integer as a function of the filter used.
#' @param e1,e2 lasR objects.
#' @param ... Unused.
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' gnd = keep_class(c(2,9))
#' reader_las(gnd)
#' triangulate(filter = keep_ground())
#' rasterize(1, "max", filter = "Z > 5")
#' @name filters
#' @rdname filters
#' @md
NULL

#' @rdname filters
#' @export
keep_class = function(x) { make_filter(paste("Classification %in%", paste(x, collapse = " "))) }

#' @rdname filters
#' @export
drop_class = function(x) { make_filter(paste("Classification %out%", paste(x, collapse = " "))) }

#' @rdname filters
#' @export
keep_first = function() { make_filter("ReturnNumber == 1")}

#' @rdname filters
#' @export
drop_first = function() { make_filter("ReturnNumber != 1")}

#' @rdname filters
#' @export
keep_ground = function() { make_filter("Classification == 2") }

#' @rdname filters
#' @export
keep_ground_and_water = function() { "Classification %in% 2 9" }

#' @rdname filters
#' @export
drop_ground = function() { make_filter("Classification != 2") }

#' @rdname filters
#' @export
keep_noise = function() { make_filter("Classification == 18") }

#' @rdname filters
#' @export
drop_noise = function() { make_filter("Classification != 18") }

#' @rdname filters
#' @export
keep_z_above = function(x) { make_filter(paste("Z >=", x[1]))}

#' @rdname filters
#' @export
drop_z_above = function(x) { make_filter(paste("Z <", x[1]))}

#' @rdname filters
#' @export
keep_z_below = function(x) { make_filter(paste("Z <", x[1]))}

#' @rdname filters
#' @export
drop_z_below = function(x) { make_filter(paste("Z >=", x[1]))}

#' @rdname filters
#' @export
drop_duplicates = function() { make_filter("-drop_duplicates")}

filter_usage <- function() { invisible(.Call(`C_lasfilterusage`)) }

transform_usage <- function() { invisible(.Call(`C_lastransformusage`)) }

make_filter = function(x)
{
  class(x) <- "laslibfilter"
  x
}

#' @rdname filters
#' @export
print.laslibfilter = function(x, ...)
{
  class(x) <- "character"
  print(x)
}

#' @rdname filters
#' @export
`+.laslibfilter` <- function(e1, e2)
{
  if (!methods::is(e2, "character"))
    e2 <- make_filter(e2)

  if (!methods::is(e2, "laslibfilter"))
    stop("Both operands must be of class laslibfilter")

  ans <- c(e1, e2)
  ans <- make_filter(ans)
  return(ans)
}

validate_filter <- function(condition, allow_laslib_filter)
{
  if (length(condition) == 0L) return(condition)
  if (length(condition) > 1L) return(sapply(condition, validate_filter))
  if (condition == "") return(condition)
  if (substr(condition, 1, 1) == "-")
  {
    if (!allow_laslib_filter) stop(paste0("LASlib filter flags (", condition, ") are no longer supported except in the LAS/LAZ file reader stage"))
    return(condition)
  }
  # Regex patterns for general attributes
  attribute_patterns <- list(
    "above" = "^([A-Za-z_]+)\\s*>\\s*([0-9.]+)$",
    "below" = "^([A-Za-z_]+)\\s*<\\s*([0-9.]+)$",
    "aboveeq" = "^([A-Za-z_]+)\\s*>=\\s*([0-9.]+)$",
    "beloweq" = "^([A-Za-z_]+)\\s*<=\\s*([0-9.]+)$",
    "equal" = "^([A-Za-z_]+)\\s*==\\s*([0-9.]+)$",
    "different" = "^([A-Za-z_]+)\\s*!=\\s*([0-9.]+)$",
    "in" = "^([A-Za-z_]+)\\s*%in%\\s*([0-9.]+(?:\\s+[0-9.]+)*)$",
    "out" = "^([A-Za-z_]+)\\s*%out%\\s*([0-9.]+(?:\\s+[0-9.]+)*)$",
    "between" = "^([A-Za-z_]+)\\s*%between%\\s*([0-9.]+)\\s+([0-9.]+)$"
  )

  attribute_mapping <- list(
    "x" = c("X", "x"),
    "y" = c("Y", "y"),
    "z" = c("Z", "z"),
    "intensity" = c("Intensity", "intensity", "i"),
    "class" = c("Classification", "classification", "class"),
    "userdata" = c("UserData", "userdata", "user_data", "ud"),
    "psid" = c("PointSourceID", "point_source", "point_source_id", "pointsourceid", "psid"),
    "gpstime" = c("gpstime", "gps_time", "GPStime", "t", "time", "gps"),
    "angle" = c("angle", "Angle", "ScanAngle", "ScanAngleRank", "scan_angle", "a"),
    "return" = c("ReturnNumber", "return", "return_number", "returnnumber")
  )

  operators = names(attribute_patterns)
  attributes = names(attribute_mapping)

  # Function to map attribute to standardized name
  map_attribute <- function(attribute)
  {
    for (standard_name in names(attribute_mapping))
    {
      if (attribute %in% attribute_mapping[[standard_name]])
      {
        return(standard_name)
      }
    }
    return (attribute)
  }

  # Iterate over patterns to match and generate the corresponding flag
  for (type in names(attribute_patterns))
  {
    pattern <- attribute_patterns[[type]]
    if (grepl(pattern, condition))
    {
      match <- regmatches(condition, regexec(pattern, condition))

      attribute <- map_attribute(match[[1]][2])  # Map to standardized attribute name
      values <- match[[1]][3:length(match[[1]])]

      # Generate flags based on the match type
      if (type == "above") {
        return(paste0("-lasr_", attribute, "_above ", values[1]))
      }
      if (type == "aboveeq") {
        return(paste0("-lasr_", attribute, "_aboveeq ", values[1]))
      }
      if (type == "below") {
        return(paste0("-lasr_", attribute, "_below ", values[1]))
      }
      if (type == "beloweq") {
        return(paste0("-lasr_", attribute, "_beloweq ", values[1]))
      }
      if (type == "between") {
        return(paste0("-lasr_", attribute, "_between ", values[1], " ", values[2]))
      }
      if (type == "equal") {
        return(paste0("-lasr_", attribute, "_equal ", values[1]))
      }
      if (type == "different") {
        return(paste0("-lasr_", attribute, "_different ", values[1]))
      }
      if (type == "in") {
        # Handle %in% operator and convert the list of values
        value_list <- strsplit(values[1], ",")[[1]]
        value_list <- paste(trimws(value_list), collapse = ", ")  # Clean up spaces around values
        return(paste0("-lasr_", attribute, "_in ", value_list))
      }
      if (type == "out") {
        # Handle %out% operator and convert the list of values
        value_list <- strsplit(values[1], ",")[[1]]
        value_list <- paste(trimws(value_list), collapse = ", ")  # Clean up spaces around values
        return(paste0("-lasr_", attribute, "_out ", value_list))
      }
    }
  }

  # If no matching condition
  stop(paste("Condition", condition, "not recognized."))
}

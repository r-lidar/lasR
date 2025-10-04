#' @param use_attribute character. Specifies the attribute to use for the operation, with "Z" (the coordinate) as the default.
#' Alternatively, this can be the name of any other attribute, such as "Intensity", "gpstime", "ReturnNumber", or "HAG", if it exists.
#' Note: The function does not fail if the specified attribute does not exist in the point cloud.
#' For example, if "Intensity" is requested but not present, or "HAG" is specified but unavailable,
#' the internal engine will return 0 for the missing attribute.
#' @md

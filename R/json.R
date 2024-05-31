# Simplified from rjson package
# https://github.com/alexcb/rjson/blob/main/rjson/R/json.R
toJSON <- function(x)
{
  # convert factors to characters
  if (is.factor(x))
  {
    tmp_names <- names(x)
    x = as.character(x)
    names(x) <- tmp_names
  }

  if (!is.vector(x) && !is.null(x) && !is.list(x))
  {
    x <- as.list( x )
    warning("JSON only supports vectors and lists")
  }

  if (is.null(x))
  {
    return("null")
  }

  # treat named vectors as lists
  if (!is.null(names(x)))
  {
    x <- as.list(x)
  }

  # named lists only
  if (is.list(x) && !is.null(names(x)))
  {
    if (any(duplicated(names(x))))
    {
      stop("A JSON list must have unique names");
    }

    str = "{\n"
    first_elem = TRUE
    for(n in names(x))
    {
      if (first_elem )
        first_elem = FALSE
      else
        str = paste0(str, ',\n')

      str = paste0(str, deparse(n), ":", toJSON(x[[n]]))
    }
    str = paste0( str, "\n}")

    return(str)
  }

  # treat lists without names as JSON array
  if (length(x) != 1 || is.list(x))
  {
    if (!is.null(names(x)))
    {
      return(toJSON(as.list(x))) #vector with names - treat as JSON list
    }

    str = "[\n"
    first_elem = TRUE
    for (val in x)
    {
      if (first_elem )
        first_elem = FALSE
      else
        str = paste0(str, ',\n')

      str = paste0(str, toJSON(val))
    }

    str = paste0(str, "\n]")

    return(str)
  }

  if (is.nan(x))
    return("\"NaN\"" )

  if (is.na(x))
    return("\"NA\"" )

  if (is.infinite(x))
    return(ifelse( x == Inf, "\"Inf\"", "\"-Inf\"" ))

  if (is.logical(x))
    return(ifelse(x, "true", "false"))

  if (is.character(x))
    return(deparse(x))

  if (is.numeric(x))
    return(as.character(x))

  stop("shouldnt make it here - unhandled type not caught" )
}
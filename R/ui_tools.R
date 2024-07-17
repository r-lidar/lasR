interpolate_R_expression = function(str)
{
  pattern <- "#(.*?)#"

  # Use regexpr to find matches
  matches <- gregexpr(pattern, str)

  # Extract the matches using regmatches
  extracted <- regmatches(str, matches)

  evaluated <- sapply(extracted[[1]], function(x) {
    cleaned <- gsub('\\\\\"', '\"', x)  # Clean the string
    eval(parse(text = gsub('#', '', cleaned)))  # Remove # and evaluate
  })

  for (i in seq_along(evaluated)) {
    str <- sub(pattern, evaluated[i], str, fixed = FALSE)
  }

  str
}

#' @param filter the 'filter' argument allows filtering of the point-cloud to work with points of
#' interest. The available filters are those from LASlib and can be found by running `print_filters()`.
#' For a given algorithm when a filter is applied, only the points that meet the criteria are processes.
#' The most common strings are "-keep_first", "-keep_class 2", "drop_z_below 2". For more details see
#' \link{filters}.

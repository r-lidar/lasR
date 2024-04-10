#' Normalize the point cloud
#'
#' Normalize the point cloud using \link{triangulate} and \link{transform_with}
#'
#' @param extrabytes bool. If FALSE the coordinate Z of the point cloud is modified and becomes the
#' height above ground (HAG). If TRUE the coordinate Z is not modified and a new extrabytes attribute
#' named 'HAG' is added to the point cloud.
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' pipeline <- reader_las() + normalize() + write_las()
#' exec(pipeline, on = f)
#' @seealso
#' \link{triangulate}
#' \link{transform_with}
#' @export
#' @md
normalize = function(extrabytes = FALSE)
{
  tri <- triangulate(filter = keep_ground())
  pipeline <- tri

  if (extrabytes)
  {
    extra <- add_extrabytes("int", "HAG", "Height Above Ground")
    trans <- transform_with(tri, store_in_attribute = "HAG")
    pipeline <- pipeline + extra + trans
  }
  else
  {
    trans <- transform_with(tri)
    pipeline <- pipeline + trans
  }

  return(pipeline)
}


#' Digital Terrain Model
#'
#' Create a Digital Terrain Model using \link{triangulate} and \link{rasterize}.
#'
#' @param res numeric. The resolution of the raster.
#' @param add_class integer. By default it triangulates using ground points (class 2). It is possible
#' to provide additional classes such as 9 for water.
#' @template param-ofile
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' pipeline <- reader_las() + dtm()
#' exec(pipeline, on = f)
#' @seealso
#' \link{triangulate}
#' \link{rasterize}
#' @export
dtm = function(res = 1, add_class = NULL, ofile = tempfile(fileext = ".tif"))
{
  filter = keep_ground()
  if (!is.null(add_class)) filter <- filter + keep_class(add_class)
  tin <- triangulate(filter = filter)
  chm <- rasterize(res, tin, ofile = ofile)
  return(tin+chm)
}

#' Canopy Height Model
#'
#' Create a Canopy Height Model using \link{triangulate} and \link{rasterize}.
#'
#' @param res numeric. The resolution of the raster.
#' @param tin bool. By default the CHM is a point-to-raster based methods i.e. each pixel is
#' assigned the elevation of the highest point. If `tin = TRUE` the CHM is a triangulation-based
#' model. The first returns are triangulated and interpolated.
#' @template param-ofile
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' pipeline <- reader_las() + chm()
#' exec(pipeline, on = f)
#' @seealso
#' \link{triangulate}
#' \link{rasterize}
#' @export
#' @md
chm = function(res = 1, tin = FALSE, ofile = tempfile(fileext = ".tif"))
{
  if (tin)
  {
    tin <- triangulate(filter = keep_first())
    chm_tin <- rasterize(res, tin, ofile = ofile)
    pipeline <- tin + chm_tin
  }
  else
  {
    pipeline <- rasterize(res, "max", ofile = ofile)
  }

  return(pipeline)
}
#' Height Above Ground (HAG)
#'
#' Normalize the point cloud using \link{triangulate} and \link{transform_with}. This process involves
#' triangulating the ground points and then using `transform_with` to linearly interpolate the elevation
#' for each point within the corresponding triangles. The `normalize()` function modifies the Z elevation
#' values, effectively flattening the topography and normalizing the point cloud based on Height Above Ground (HAG).
#' In contrast, the `hag()` function records the HAG in an extrabyte attribute named 'HAG', while preserving
#' the original Z coordinates (Height Above Sea Level).

#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' pipeline <- reader_las() + normalize() + write_las()
#' exec(pipeline, on = f)
#' @seealso
#' \link{triangulate}
#' \link{transform_with}
#' @export
#' @md
#' @rdname hag
normalize = function()
{
  tri <- triangulate(filter = keep_ground_and_water())
  trans <- transform_with(tri)
  pipeline <- tri + trans
  return(pipeline)
}

#' @rdname hag
#' @export
hag = function()
{
  tri <- triangulate(filter = keep_ground_and_water())
  extra <- add_extrabytes("int", "HAG", "Height Above Ground")
  trans <- transform_with(tri, store_in_attribute = "HAG")
  pipeline <- tri + extra + trans
}


#' Digital Terrain Model
#'
#' Create a Digital Terrain Model using \link{triangulate} and \link{rasterize}.
#'
#' @param res numeric. The resolution of the raster.
#' @param add_class integer. By default it triangulates using ground and water points (classes 2 and 9).
#' It is possible to provide additional classes.
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
dtm = function(res = 1, add_class = NULL, ofile = temptif())
{
  filter = keep_ground_and_water()
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
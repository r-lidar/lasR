# ===== A =====

#' Add attributes to a point cloud
#'
#' Modify the memory layout of the point cloud to add an attributes to a point cloud. Values are
#' zeroed: the underlying point cloud is edited to support a new extrabyte attribute. This new
#' attribute can be populated later in another stage
#'
#' @param name character. The name of the attribute to add or remove to the file.
#' @param names character vector. The names of attribute to add or remove to the file.
#' @param description character. A short description of the extra bytes attribute to add to the file (32 characters).
#' @param data_type character. The data type of the extra bytes attribute. Can be "uchar", "char", "ushort",
#' "short", "uint", "int", "uint64", "int64", "float", "double".
#' @param scale,offset numeric. The scale and offset of the data. See LAS specification. Leave unchanged
#' if not working with LAS files.
#' @template return-pointcloud
#' @export
#' @rdname add_attribute
#' @examples
#' f <- system.file("extdata", "Example.las", package = "lasR")
#' fun <- function(data) { data$RAND <- runif(nrow(data), 0, 100); return(data) }
#' pipeline <- reader() +
#'   add_extrabytes("float", "RAND", "Random numbers") +
#'   callback(fun, expose = "xyz")
#' exec(pipeline, on = f)
add_extrabytes = function(data_type, name, description, scale = 1, offset = 0)
{
  return(.APISTAGES$add_attribute(data_type, name, description, scale, offset))
}

#' Add/remove RGB attributes to a LAS file
#'
#' Modifies the LAS format to convert into a format with RGB attributes. Values are zeroed: the underlying
#' point cloud is edited to be transformed in a format that supports RGB. RGB can be populated later
#' in another stage. If the point cloud already has RGB, nothing happens, RGB values are preserved.
#'
#' @return If this stage transforms the point cloud in the pipeline it returns nothing. Otherwise
#' it returns the R object returned by the function 'fun'
#'
#' @examples
#' f <- system.file("extdata", "Example.las", package="lasR")
#'
#' pipeline <- add_rgb() + write_las()
#' exec(pipeline, on = f)
#' @export
add_rgb = function()
{
  return(.APISTAGES$add_rgb())
}

#' Aggregate a point cloud and compute metrics for each group
#'
#' Aggregate a point cloud and compute metrics for each group. The currently supported grouping method
#' is per pixel with a raster output. This is the stage returned when using \link{rasterize} with
#' a user-defined expression. At this stage, this stage only aims to drive \link{rasterize}.
#'
#' @param res numeric. The resolution of the raster.
#' @param call expression. User-defined expression.
#' @template param-filter
#' @template param-ofile
#' @template return-raster
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#' pipeline <- summarise()
#' ans <- exec(pipeline, on = f)
#' ans
#' @seealso
#' \link{rasterize}
#' @noRd
aggregate = function(res, call, filter = "", ofile = temptif(), ...)
{
  call <- substitute(call)
  env <- new.env(parent=parent.frame())
  aggregate_q(res, call, filter, ofile, env, ...)
}

aggregate_q = function(res, call, filter, ofile, env, ...)
{
  res_raster  <- res[1]
  res_window <- res[1]
  if (length(res) > 1L)
    res_window <- res[2]

  call <- as.call(call)

  # evaluate the number of metrics returned by user's expression to be able to initialize
  # a raster with the correct number of bands.
  dots <- list(...)
  nmetrics <- dots$nmetrics
  if (is.null(nmetrics))
  {
    nmetrics <- eval_number_of_metrics(call, env)
    if (is.na(nmetrics)) stop(paste0("Cannot evaluate the number of metrics returned by ", deparse(call)))
  }

  call_addr = .APIOPERATIONS$get_address(call)
  env_addr = .APIOPERATIONS$get_address(env)

  s = .APISTAGES$aggregate(res_raster, nmetrics, res_window, call_addr, env_addr, filter, ofile)

  # Ensure these are not garbage collected by registering attributes
  attr(s, "call") = call
  attr(s, "env") = env

  return(s)
}

# ===== C =====


#' Call a user-defined function on the point cloud
#'
#' Call a user-defined function on the point cloud. The function receives a `data.frame` with the
#' point cloud. Its first input must be the point cloud. If the function returns anything other than
#' a `data.frame` with the same number of points, the output is stored and returned at the end. However,
#' if the output is a `data.frame` with the same number of points, it updates the point cloud. This
#' function can, therefore, be used to modify the point cloud using a user-defined function. The function
#' is versatile but complex. A more comprehensive set of examples can be found in the
#' [online tutorial](https://r-lidar.github.io/lasR/articles/tutorial.html#callback).
#'
#' In `lasR`, the point cloud is not exposed to R in a `data.frame` like in lidR. It is stored internally
#' in a C++ structure and cannot be seen or modified directly by users using R code. The `callback` function
#' is the only stage that allows direct interaction with the point cloud by **copying** it
#' temporarily into a `data.frame` to apply a user-defined function.\cr\cr
#' **expose:** the 'expose' argument specifies the data that will actually be exposed to R. For example,
#' 'xyzia' means that the x, y, and z coordinates, the intensity, and the scan angle will be exposed.
#' The supported entries are t - gpstime, a - scan angle, i - intensity, n - number of returns,
#' r - return number, c - classification, u - user data, p - point source ID, e - edge of flight line flag,
#' R - red channel of RGB color, G - green channel of RGB color, B - blue channel of RGB color,
#' N - near-infrared channel, C - scanner channel (format 6+)
#' Also numbers from 1 to 9 for the extra attributes data numbers 1 to 9. 'E' enables all extra attribute to be
#' loaded. '*' is the wildcard that enables everything to be exposed from the point cloud
#'
#' @param fun function. A user-defined function that takes as first argument a `data.frame` with the exposed
#' point cloud attributes (see examples).
#' @param expose character. Expose only attributes of interest to save memory (see details).
#' @param drop_buffer bool. If false, does not expose the point from the buffer.
#' @param no_las_update bool. If the user-defined function returns a data.frame, this is supposed to
#' update the point cloud. Can be disabled.
#' @param ... parameters of function `fun`
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#'
#' # There is no function in lasR to read the data in R. Let's create one
#' read_las <- function(f)
#' {
#'   load <- function(data) { return(data) }
#'   read <- reader()
#'   call <- callback(load, expose = "xyzi", no_las_update = TRUE)
#'   return (exec(read + call, on = f))
#' }
#' las <- read_las(f)
#' head(las)
#'
#' convert_intensity_in_range <- function(data, min, max)
#' {
#'   i <- data$Intensity
#'   i <- ((i - min(i)) / (max(i) - min(i))) * (max - min) + min
#'   i[i < min] <- min
#'   i[i > max] <- max
#'   data$Intensity <- as.integer(i)
#'   return(data)
#' }
#'
#' read <- reader()
#' call <- callback(convert_intensity_in_range, expose = "i", min = 0, max = 255)
#' write <- write_las()
#' pipeline <- read + call + write
#' ans <- exec(pipeline, on = f)
#'
#' las <- read_las(ans)
#' head(las)
#'
#' @seealso
#' \link{write_las}
#' @export
#' @md
callback = function(fun, expose = "xyz", ..., drop_buffer = FALSE, no_las_update = FALSE)
{
  args <- list(...)
  fargs <- formals(fun)
  fargs <- fargs[-1]
  fargs[names(args)] <- args

  func_addr = .APIOPERATIONS$get_address(fun)
  args_addr = .APIOPERATIONS$get_address(fargs)

  s = .APISTAGES$callback(func_addr, args_addr, expose, drop_buffer, no_las_update)

  # Ensure these are not garbage collected by registering attributes
  attr(s, "func") = fun
  attr(s, "args") = fargs

  return(s)
}

#' Classify noise points
#'
#' Classify points using the Statistical Outliers Removal (SOR) methods first described in the PCL
#' library and also implemented in CloudCompare (see references). For each point, it computes the mean
#' distance to all its k-nearest neighbors. The points that are farther than the average distance
#' plus a number of times (multiplier) the standard deviation are considered noise.
#'
#' @param k	numeric. The number of neighbours
#' @param m numeric. Multiplier. The maximum distance will be: ⁠avg distance + m * std deviation⁠
#' @param class integer. The class to assign to the points that match the condition.
#'
#' @template return-pointcloud
#'
#' @export
classify_with_sor = function(k = 8, m = 6, class = 18L) { .APISTAGES$classify_with_sor(k, m, class) }

#' Classify noise points
#'
#' Classify points using Isolated Voxel Filter (IVF). The stage identifies points that have only a few other
#' points in their surrounding 3 x 3 x 3 = 27 voxels and edits the points to assign a target classification.
#' Used with class 18, it classifies points as noise. This stage modifies the point cloud in the pipeline
#' but does not produce any output.
#'
#' @param res numeric. Resolution of the voxels.
#' @param n integer. The maximal number of 'other points' in the 27 voxels.
#' @param class integer. The class to assign to the points that match the condition.
#' @template return-pointcloud
#'
#' @export
classify_with_ivf = function(res = 5, n = 6L, class = 18L) { .APISTAGES$classify_with_ivf(res, n, class) }

#' Classify ground points
#'
#' Classify points using the Cloth Simulation Filter by Zhang et al. (2016) (see references) that relies
#' on the authors' original source code. If the point cloud already has ground points, the classification
#' of the original ground point is set to zero. This stage modifies the point cloud in the pipeline but
#' does not produce any output.
#'
#' @param slope_smooth logical. When steep slopes exist, set this parameter to TRUE to reduce
#' errors during post-processing.
#' @param class_threshold scalar. The distance to the simulated cloth to classify a point cloud into ground
#' and non-ground. The default is 0.5.
#' @param cloth_resolution scalar. The distance between particles in the cloth. This is usually set to the
#' average distance of the points in the point cloud. The default value is 0.5.
#' @param rigidness integer. The rigidness of the cloth. 1 stands for very soft (to fit rugged
#' terrain), 2 stands for medium, and 3 stands for hard cloth (for flat terrain). The default is 1.
#' @param iterations integer. Maximum iterations for simulating cloth. The default value is 500. Usually,
#' there is no need to change this value.
#' @param time_step scalar. Time step when simulating the cloth under gravity. The default value
#' is 0.65. Usually, there is no need to change this value. It is suitable for most cases.
#' @param class integer. The classification to attribute to the points. Usually 2 for ground points.
#' @param ... Unused
#' @template param-filter
#'
#' @template return-pointcloud
#'
#' @references
#' W. Zhang, J. Qi*, P. Wan, H. Wang, D. Xie, X. Wang, and G. Yan, “An Easy-to-Use Airborne LiDAR Data
#' Filtering Method Based on Cloth Simulation,” Remote Sens., vol. 8, no. 6, p. 501, 2016.
#' (http://www.mdpi.com/2072-4292/8/6/501/htm)
#'
#' @export
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' pipeline = classify_with_csf(TRUE, 1 ,1, time_step = 1) + write_las()
#' ans = exec(pipeline, on = f, progress = TRUE)
classify_with_csf = function(slope_smooth = FALSE, class_threshold = 0.5, cloth_resolution = 0.5, rigidness = 1L, iterations = 500L, time_step = 0.65, ..., class = 2L, filter = "")
{
  .APISTAGES$classify_with_csf(slope_smooth, class_threshold, cloth_resolution, rigidness, iterations, time_step, class, filter)
}

# ===== G =====

#' Compute pointwise geometry features
#'
#' Compute pointwise geometry features based on local neighborhood. Each feature is added into a new point
#' attribute. The names of the new attributes (if recorded) are `coeff00`, `coeff01`,
#' `coeff02` and so on, `lambda1`, `lambda2`, `lambda3`, `anisotropy`, `planarity`, `sphericity`, `linearity`,
#' `omnivariance`, `curvature`, `eigensum`, `angle`, `normalX`, `normalY`, `normalZ` (recorded in this order).
#' There is a total of 23 attributes that can be added. It is strongly discouraged to use them all.
#' All the features are recorded with single precision floating points yet computing them all will triple
#' the size of the point cloud. This stage modifies the point cloud in the pipeline but does not produce
#' any output. If a pipeline has two or more stages with this stage, then attribute with the same name are
#' overwritten.
#'
#' @param k,r integer and numeric respectively for k-nearest neighbours and radius of the neighborhood
#' sphere. If k is given and r is missing, computes with the knn, if r is given and k is missing
#' computes with a sphere neighborhood, if k and r are given computes with the knn and a limit on the
#' search distance.
#' @param features String. Geometric feature to export. Each feature is added into a new
#' attribute. Use 'C' for the 9 principal component coefficients, 'E' for the 3 eigenvalues of the
#' covariance matrix, 'a' for anisotropy, 'p' for planarity, 's' for sphericity, 'l' for linearity,
#' 'o' for omnivariance, 'c' for curvature, 'e' for the sum of eigenvalues, 'i' for the angle
#' (inclination in degrees relative to the vertical), and 'n' for the 3 components of the normal vector.
#' Notice that the uppercase labeled components allow computing all the lowercase labeled components.
#' Default is "". In this case, the singular value decomposition is computed but serves no purpose.
#' The order of the flags does not matter and the features are recorded in the order mentioned above.
#'
#' @template return-pointcloud
#'
#' @export

#' @references Hackel, T., Wegner, J. D., & Schindler, K. (2016). Contour detection in unstructured 3D
#' point clouds. In Proceedings of the IEEE conference on computer vision and pattern recognition (pp. 1610-1618).
#' @md
#' @examples
#' f <- system.file("extdata", "Example.las", package = "lasR")
#' pipeline <- geometry_features(8, features = "pi") + write_las()
#' ans <- exec(pipeline, on = f)
geometry_features = function(k, r, features = "")
{
  if (missing(k) && missing(r))  stop("'k' and 'r' are missing", call. = FALSE)
  if (!missing(r) && !missing(k)) { } # knn + radius
  if (!missing(k) && missing(r))  { r <- 0 }  # knn
  if (!missing(r) && missing(k)) { k <- 0 }   # radius
  .APISTAGES$geometry_feature(k, r, features)
}


# ===== D =====

#' Filter and delete points
#'
#' This stage modifies the point cloud in the pipeline but does not produce any output. Points
#' matching the `filter` criteria are **processed**. In this case, it means they are deleted.
#' **Note:** In versions < 0.17, the behavior was the opposite.
#'
#' @template param-filter
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Megaplot.las", package="lasR")
#' read <- reader()
#' filter <- delete_points("Z < 4") # Remove points below 4
#'
#' pipeline <- read + summarise() + filter + summarise()
#' exec(pipeline, on = f)
#' @export
delete_points = function(filter = "") { .APISTAGES$delete_points(filter) }

#' @export
#' @rdname delete_points
delete_noise = function() { delete_points(keep_noise()) }

#' @export
#' @rdname delete_points
delete_ground = function() { delete_points(keep_ground()) }

# ===== E =====

#' Edit an attribute of the points
#'
#' Edit an attribute of the points by filtering the point based on criteria.
#' @template param-filter
#' @param attribute string. The name of an attribute to edit
#' @param value numeric. The value to assign. Be careful, if the user try to assign a value out of
#' range of representable value for a given data type it will be clamped.
#' @export
#' @examples
#' f <- system.file("extdata", "Example.las", package="lasR")
#'
#' edit = edit_attribute(filter = c("Z < 975", "Z > 974"), attribute = "UserData", value = 2)
#' io = write_las(templas())
#' pipeline = edit + io
#' ans = exec(pipeline, on = f)
#' @template return-pointcloud
edit_attribute = function(filter = "", attribute = "", value = 0) { .APISTAGES$edit_attribute(filter, attribute, value) }

# ===== F =====

#' Select highest or lowest points
#'
#' Select and retained only highest or lowest points per grid cell
#'
#' @param res numeric. The resolution of the grid
#' @param operator string. Can be min or max to retain lowest or highest points
#' @template param-filter
#' @md
#' @export
filter_with_grid = function(res, operator = "min", filter = "") { .APISTAGES$filter_with_grid(res, operator, filter) }

#' Calculate focal ("moving window") values for each cell of a raster
#'
#' Calculate focal ("moving window") values for each cell of a raster using various functions. NAs
#' are always omitted; thus, this stage effectively acts as an NA filler. The window is always circular.
#' The edges are handled by adjusting the window.
#'
#' @param raster LASRalgorithm. A stage that produces a raster.
#' @param size numeric. The window size **in the units of the point cloud**, not in pixels. For example, 2 means 2 meters
#' or 2 feet, not 2 pixels.
#' @param fun string. Function to apply. Supported functions are 'mean', 'median', 'min', 'max', 'sum'.
#' @template param-ofile
#' @template return-raster
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#'
#' chm = rasterize(2, "zmax")
#' chm2 = lasR:::focal(chm, 8, fun = "mean")
#' chm3 = lasR:::focal(chm, 8, fun = "max")
#' pipeline <- reader() + chm + chm2 + chm2
#' ans = exec(pipeline, on = f)
#'
#' terra::plot(ans[[1]])
#' terra::plot(ans[[2]])
#' terra::plot(ans[[3]])
#' @export
focal = function(raster, size, fun = "mean", ofile = temptif())
{
  info = .APIOPERATIONS$get_stage_info(raster)
  if (!info$raster) stop("'raster' must be a raster stage")  # nocov
  ofile = normalizePath(ofile, mustWork = FALSE)

  .APISTAGES$focal(info[["uid"]], size, fun, ofile)
}

# ===== H =====

#' Contour of a point cloud
#'
#' This stage uses a Delaunay triangulation and computes its contour. The contour of a strict
#' Delaunay triangulation is the convex hull, but in lasR, the triangulation has a `max_edge` argument.
#' Thus, the contour might be a convex hull with holes. Used without triangulation it returns the bouding
#' box of the points.
#'
#' @param mesh NULL or LASRalgorithm. A `triangulate` stage. If NULL take the bounding box of the
#' header of each file.
#' @template param-ofile
#'
#' @template return-vector
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#' read <- reader()
#' tri <- triangulate(20, filter = keep_ground())
#' contour <- hulls(tri)
#' pipeline <- read + tri + contour
#' ans <- exec(pipeline, on = f)
#' plot(ans)
#'
#' @seealso
#' \link{triangulate}
#'
#' @export
#' @md
hulls = function(mesh = NULL, ofile = tempgpkg())
{
  if (!is.null(mesh))
  {
    info = .APIOPERATIONS$get_stage_info(mesh)
    if (info$name != "triangulate") stop("the stage must be 'triangulate'")  # nocov
    return(.APISTAGES$hull_triangulation(info[["uid"]], ofile))
  }
  else
  {
    return(.APISTAGES$hull(ofile))
  }
}

# ==== I ======

#' Print Information about the Point Cloud
#'
#' This function prints useful information about point cloud files, including the file version, size,
#' bounding box, CRS, and more. When called without parameters, it returns a pipeline stage. For
#' convenience, it can also be called with the path to a file for immediate execution, which is likely
#' the most common use case (see examples).
#'
#' @param f string (optional) Path to a LAS/LAZ file.
#'
#' @return nothing. The stage is used for its side effect of printing
#'
#' @export
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package = "lasR")
#' g <- system.file("extdata", "Example.pcd", package = "lasR")
#'
#' # Return a pipeline stage
#' exec(info(), on = f)
#'
#' # Convenient user-friendly usage
#' info(f)
#'
#' info(g)
info = function(f)
{
  if (missing(f))  {
    return(.APISTAGES$info())
  } else {
    exec(info(), on = f)
    return(invisible())
  }
}

# ===== K ====

#' @export
#' @rdname add_attribute
keep_attributes = function(names){ .APISTAGES$keep_attributes(names) }

# ===== L =====

#' Load a raster for later use
#'
#' Load a raster from a disk file for later use. For example, load a DTM to feed the \link{transform_with}
#' stage or load a CHM to feed the \link{pit_fill} stage. The raster is never loaded entirely. Internally, only
#' chunks corresponding to the currently processed point cloud are loaded. Be careful: internally, the raster
#' is read as float no matter the original datatype.
#'
#' @param file character. Path to a raster file.
#' @param band integer. The band to load. It reads and loads only a single band.
#' @export
#' @examples
#' r <- system.file("extdata/bcts", "bcts_dsm_5m.tif", package = "lasR")
#' f <- paste0(system.file(package = "lasR"), "/extdata/bcts/")
#' f <- list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
#'
#' # In the following pipeline, neither load_raster nor pit_fill process any points.
#' # The internal engine is capable of knowing that, and the LAS files won't actually be
#' # read. Yet the raster r will be processed by chunk following the LAS file pattern.
#' rr <- load_raster(r)
#' pipeline <- rr + pit_fill(rr)
#' ans <- exec(pipeline, on = f, verbose = FALSE)
load_raster = function(file, band = 1L)
{
  file <- normalizePath(file)
  .APISTAGES$load_raster(file, band)
}

#' Load a matrix for later use
#'
#' Load a matrix for later use. For example, load a matrix to feed the \link{transform_with}
#' stage
#'
#' @param matrix a 4x4 matrix typically a Rotation-Translation Matrix (RTM)
#' @param check Boolean. Check matrix orthogonality.
#'
#' @examples
#' a = 20 * pi / 180
#' m <- matrix(c(
#'   cos(a), -sin(a), 0, 1000,
#'   sin(a), cos(a), 0, 0,
#'   0, 0, 1, 0,
#'   0, 0, 0, 1), nrow = 4, byrow = TRUE)
#'
#' mat = load_matrix(m)
#' trans = transform_with(mat)
#' write = write_las(tempfile(fileext = ".las"))
#' pipeline = mat + trans + write
#'
#' f <- system.file("extdata", "Topography.las", package="lasR")
#'
#' exec(pipeline, on = f)
#' @export
load_matrix = function(matrix, check = TRUE)
{
  if (!is.matrix(matrix)) stop("'matrix' is not a matrix")
  if (dim(matrix)[1] != 4) stop("'matrix' is not a 4x4 matrix")
  if (dim(matrix)[2] != 4) stop("'matrix' is not a 4x4 matrix")
  .APISTAGES$load_matrix(as.numeric(t(matrix)), check)
}

#' Local Maximum
#'
#' The Local Maximum stage identifies points that are locally maximum. The window size is
#' fixed and circular. This stage does not modify the point cloud. It produces a derived product
#' in vector format. The function `local_maximum_raster` applies on a raster instead of the point cloud
#'
#' @param ws numeric. Diameter of the moving window used to detect the local maxima in the units of
#' the input data (usually meters).
#' @param min_height numeric. Minimum height of a local maximum. Threshold below which a point cannot be a
#' local maximum. Default is 2.
#' @template param-attribute
#' @param raster LASRalgorithm. A stage that produces a raster.
#' @param record_attributes The coordinates XYZ of points corresponding to the local maxima are recorded.
#' It is also possible to record the attributes of theses points such as the intensity, return number, scan
#' angle and so on.
#' @param store_in_attribute In addition to producing a geospatial file with the local maxima,
#' the points can also be flagged: 0 if the point is not a local maximum, and 1 if the
#' point is a local maximum. If the attribute does not exist, it must first be created
#' with \link{add_extrabytes} (see examples).

#'
#' @template param-filter
#' @template param-ofile
#'
#' @template return-vector
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package = "lasR")
#' read <- reader()
#' lmf <- local_maximum(5)
#' ans <- exec(read + lmf, on = f)
#' ans
#'
#' chm <- rasterize(1, "max")
#' lmf <- local_maximum_raster(chm, 5)
#' ans <- exec(read + chm + lmf, on = f)
#' # terra::plot(ans$rasterize)
#' # plot(ans$local_maximum, add = T, pch = 19)
#'
#' # Storing LM in UserData.
#' lmf <- local_maximum(5, store_in_attribute = "UserData")
#' ans <- exec(read + lmf + write_las(), on = f)
#' ans
#'
#' # Storing in an new attribute without geospatial output
#' attr <- add_extrabytes("uchar", "lm", "local maximum flag")
#' lmf <- local_maximum(5, ofile = "", store_in_attribute = "lm")
#' ans <- exec(attr + lmf + write_las(), on = f)
#' ans
#' @export
#' @md
local_maximum = function(ws, min_height = 2, filter = "", ofile = tempgpkg(), use_attribute = "Z", record_attributes = FALSE, store_in_attribute = "")
{
  return(.APISTAGES$local_maximum(ws, min_height, filter,  ofile, use_attribute, record_attributes, store_in_attribute))
}

#' @export
#' @rdname local_maximum
local_maximum_raster = function(raster, ws, min_height = 2, filter = "", ofile = tempgpkg())
{
  info = .APIOPERATIONS$get_stage_info(raster)
  if (!info$raster)  stop("the stage must be a raster stage")
  ofile = normalizePath(ofile, mustWork = FALSE)

  .APISTAGES$local_maximum_raster(connect = info[["uid"]], ws, min_height, filter, ofile)
}

# ===== N ====

#' Compute metrics for a neighborhood
#'
#' This stage calculates specified metrics for a given neighborhood. Currently the neighborhood to be
#' "local_maximum" stage. For each local maximum found by the local maximum stage, it searches for the
#' points in neighborhood and computes metrics using these points.
#'
#' @section Operators:
#' Each string is composed of two parts separated by an underscore. The first part is the attribute
#'  on which the metric must be computed (e.g., z, intensity, classification).
#' The second part is the name of the metric (e.g., mean, sd, cv). A string thus typically looks like
#' `"z_max"`, `"intensity_min"`, `"z_mean"`, `"classification_mode"`.\cr\cr
#' The available attributes are accessible via a single letter or via their lowercase name: t - gpstime,
#' a - angle, i - intensity, n - numberofreturns, r - returnnumber, c - classification,
#' s - synthetic, k - keypoint, w - withheld, o - overlap (format 6+), u - userdata, p - pointsourceid,
#' e - edgeofflightline, d - scandirectionflag, R - red, G - green, B - blue, N - nir.\cr\cr
#' The available metric names are: count, max, min, mean, median, sum, sd, cv, pX (percentile), aboveX, and mode.
#' Some metrics have an attribute + name + a parameter X, such as `pX` where `X` can be substituted by a number.
#' Here, `z_pX` represents the Xth percentile; for instance, `z_p95` signifies the 95th
#' percentile of z. `z_aboveX` corresponds to the percentage of points above X (sometimes called canopy cover).\cr\cr
#' It is possible to call a metric without the name of the attribute. In this case, z is the default.
#'
#' @param metrics Character vector. "min", "max" and "count" are accepted as well as many others
#' (see \link{metric_engine}). If `NULL` nothing is computed.
#' @param neighborhood Currently support only a "local_maximum" stage.
#' @param k,r integer and numeric respectively for k-nearest neighbours and radius of the neighborhood
#' sphere. If k is given and r is missing, computes with the knn, if r is given and k is missing
#' computes with a sphere neighborhood, if k and r are given computes with the knn and a limit on the
#' search distance.
#' @param ofile A file path where the output will be stored. Default is a temporary GeoPackage file.
#'
#' @template return-vector
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package = "lasR")
#' read <- reader()
#' lmf <- local_maximum(5, ofile = "")
#' nnm = lasR:::neighborhood_metrics(lmf, k = 10, metrics = c("i_mean", "count"))
#' ans <- exec(read + lmf + nnm, on = f)
#' ans
#' @noRd
neighborhood_metrics = function(neighborhood, metrics, k = 10, r = 0, ofile = tempgpkg())
{
  nn = .APIOPERATIONS$get_stage_info(neighborhood)
  if (nn$name != "local_maximum") stop("the stage must be a local_maximum stage")
  ofile = normalizePath(ofile, mustWork = FALSE)
  .APISTAGES$neighborhood_metrics(nn[["uid"]], metrics, k, r, ofile)
}

nothing = function(read = FALSE, stream = FALSE, loop = FALSE) { .APISTAGES$nothing(read, stream, loop) }

# ===== P ====

#' Pits and spikes filling
#'
#' Pits and spikes filling for raster. Typically used for post-processing CHM. This algorithm
#' is from St-Onge 2008 (see reference).
#'
#' @param raster LASRalgorithm. A stage that produces a raster.
#' @param lap_size integer. Size of the Laplacian filter kernel (integer value, in pixels).
#' @param thr_lap numeric. Threshold Laplacian value for detecting a cavity (all values above this
#' value will be considered a cavity). A positive value.
#' @param thr_spk numeric. Threshold Laplacian value for detecting a spike (all values below this
#' value will be considered a spike). A negative value.
#' @param med_size integer. Size of the median filter kernel (integer value, in pixels).
#' @param dil_radius integer. Dilation radius (integer value, in pixels).
#' @template param-ofile
#'
#' @template return-raster
#'
#' @references
#' St-Onge, B., 2008. Methods for improving the quality of a true orthomosaic of Vexcel UltraCam
#' images created using alidar digital surface model, Proceedings of the Silvilaser 2008, Edinburgh,
#' 555-562. https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=81365288221f3ac34b51a82e2cfed8d58defb10e
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package="lasR")
#'
#' reader <- reader(filter = keep_first())
#' tri <- triangulate()
#' chm <- rasterize(0.25, tri)
#' pit <- pit_fill(chm)
#' u <- exec(reader + tri + chm + pit, on = f)
#'
#' chm <- u[[1]]
#' sto <- u[[2]]
#'
#' #terra::plot(c(chm, sto), col = lidR::height.colors(25))
#' @export
pit_fill = function(raster, lap_size = 3L, thr_lap = 0.1, thr_spk = -0.1, med_size = 3L, dil_radius = 0L, ofile = temptif())
{
  info = .APIOPERATIONS$get_stage_info(raster)
  if (!info$raster) stop("'raster' must be a raster stage")  # nocov
  ofile = normalizePath(ofile, mustWork = FALSE)
  .APISTAGES$pit_fill(info[["uid"]],   lap_size, thr_lap,  thr_spk,  med_size, dil_radius, ofile)
}

# ===== R =====

#' Rasterize a point cloud
#'
#' Rasterize a point cloud using different approaches. This stage does not modify the point cloud.
#' It produces a derived product in raster format.
#'
#' @section Operators:
#' If `operators` is a string or a vector of strings: read \link{metric_engine} to see the possible strings
#' Below are some examples of valid calls:
#' ```
#' rasterize(10, c("max", "count", "i_mean", "z_p95"))
#' rasterize(10, c("z_max", "c_count", "intensity_mean", "p95"))
#' ```
#' If `operators` is an R user-defined expression, the function should return either a vector of numbers
#' or a `list` containing atomic numbers. To assign a band name to the raster, the vector or the `list`
#' must be named accordingly. The following are valid operators:
#' ```
#' f = function(x) { return(mean(x)) }
#' g = function(x,y) { return(c(avg = mean(x), med = median(y))) }
#' h = function(x) { return(list(a = mean(x), b = median(x))) }
#' rasterize(10, f(Intensity))
#' rasterize(10, g(Z, Intensity))
#' rasterize(10, h(Z))
#' ````
#'
#' @section Buffered:
#' If the argument `res` is a vector with two numbers, the first number represents the resolution of
#' the output raster, and the second number represents the size of the windows used to compute the
#' metrics. This approach is called Buffered Area Based Approach (BABA).\cr\cr
#' In classical rasterization, the metrics are computed independently for each pixel. For example,
#' predicting a resource typically involves computing metrics with a 400 square meter pixel, resulting
#' in a raster with a resolution of 20 meters. It is not possible to achieve a finer granularity with
#' this method.\cr\cr
#' However, with buffered rasterization, it is possible to compute the raster at a resolution of 10
#' meters (i.e., computing metrics every 10 meters) while using 20 x 20 windows for metric computation.
#' In this case, the windows overlap, essentially creating a moving window effect.\cr\cr
#' This option does not apply when rasterizing a triangulation, and the second value is not considered
#' in this case.
#'
#' @param res numeric. The resolution of the raster. Can be a vector with two resolutions.
#' In this case it does not correspond to the x and y resolution but to a buffered rasterization.
#' (see section 'Buffered' and examples)
#' @param operators Can be a character vector. "min", "max" and "count" are accepted as well
#' as many others (see section 'Operators'). Can also rasterize a triangulation if the input is a
#' LASRalgorithm for triangulation (see examples). Can also be a user-defined expression
#' (see example and section 'Operators').
#' @param ... `default_value` numeric. When rasterizing with an operator and a filter (e.g. `-keep_z_above 2`)
#' some pixels that are covered by points may no longer contain any point that pass the filter criteria
#' and are assigned NA. To differentiate NAs from non covered pixels and NAs from covered pixels without point that
#' pass the filter, the later case can be assigned another value such as 0.
#' @template param-filter
#' @template param-ofile
#'
#' @template return-raster
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader()
#' tri  <- triangulate(filter = keep_ground())
#' dtm  <- rasterize(1, tri) # input is a triangulation stage
#' avgi <- rasterize(10, mean(Intensity)) # input is a user expression
#' chm  <- rasterize(2, "max") # input is a character vector
#' pipeline <- read + tri + dtm + avgi + chm
#' ans <- exec(pipeline, on = f)
#' ans[[1]]
#' ans[[2]]
#' ans[[3]]
#'
#' # Demonstration of buffered rasterization
#'
#' # A good resolution for computing point density is 5 meters.
#' c0 <- rasterize(5, "count")
#'
#' # Computing point density at too fine a resolution doesn't make sense since there is
#' # either zero or one point per pixel. Therefore, producing a point density raster with
#' # a 2 m resolution is not feasible with classical rasterization.
#' c1 <- rasterize(2, "count")
#'
#' # Using a buffered approach, we can produce a raster with a 2-meter resolution where
#' # the metrics for each pixel are computed using a 5-meter window.
#' c2  <- rasterize(c(2,5), "count")
#'
#' pipeline = read + c0 + c1 + c2
#' res <- exec(pipeline, on = f)
#' terra::plot(res[[1]]/25)  # divide by 25 to get the density
#' terra::plot(res[[2]]/4)   # divide by 4 to get the density
#' terra::plot(res[[3]]/25)  # divide by 25 to get the density
#' @export
#' @md
rasterize = function(res, operators = "max", filter = "", ofile = temptif(), ...)
{
  class <- tryCatch({class(operators)[1]}, error = function(x) return("call"))

  p = list(...)
  default_value = p$default_value
  if (is.null(default_value)) default_value = -99999;

  res_raster  <- res[1]
  res_window <- res[1]
  if (length(res) > 1L)
    res_window <- res[2]

  if (class == "call")
  {
    env <- new.env(parent = parent.frame())
    return(aggregate_q(res, substitute(operators), filter, ofile, env))
  }

  if (methods::is(operators, "PipelinePtr"))
  {
    info = .APIOPERATIONS$get_stage_info(operators)
    if (info$name != "triangulate")
      stop("'operator' must be a triangulation stage")

    return(.APISTAGES$rasterize_triangulation(info[["uid"]], res, ofile))
  }
  else if (is.character(operators))
  {
    # ----------------------
    # backward compatibility
    former_operators <- c("max", "min", "count", "zmax", "zmin", "zmean", "zmedian", "zsd", "zcv", "zabove", "zp", "imax", "imin", "imean", "imedian", "isd", "icv", "iabove", "ip")
    new_operators <- c("max", "min", "count", "z_max", "z_min", "z_mean", "z_median", "z_sd", "z_cv", "z_above", "z_p", "i_max", "i_min", "i_mean", "i_median", "i_sd", "i_cv", "i_above", "i_p")
    replace_names <- function(x, old_names, new_names) {
      for (i in seq_along(old_names)) {
        x <- gsub(old_names[i], new_names[i], x)
      }
      return(x)
    }
    operators <- replace_names(operators, former_operators, new_operators)
    # ----------------------

    return(.APISTAGES$rasterize(res_raster, res_window, operators, filter, ofile, default_value))
  }
  else
  {
    stop("Invalid operators") # nocov
  }
}

#' Initialize the pipeline
#'
#' This is the first stage that must be called in each pipeline. The stage does nothing and returns
#' nothing if it is not associated to another processing stage.
#' It only initializes the pipeline. `reader()` is the main function that dispatches into to other
#' functions. `reader_coverage()` processes the entire point cloud. `reader_circles()` and
#' `reader_rectangles()` read and process only some selected regions of interest. If the chosen
#' reader has no options i.e. using `reader()` it can be omitted.
#'
#' @template param-filter
#' @param xc,yc,r numeric. Circle centres and radius or radii.
#' @param xmin,ymin,xmax,ymax numeric. Coordinates of the rectangles
#' @param select character. Unused. Reserved for future versions.
#' @param copc_depth integer. If the files are COPC file is is possible to read the point hierarchy
#' up to a given level. COPC hierarchy is 0-index. The first level is 0 not 1.
#' @param ... passed to other readers
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#'
#' pipeline <- reader() + rasterize(10, "zmax")
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#'
#' pipeline <- reader(filter = keep_z_above(1.3)) + rasterize(10, "zmean")
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#'
#' # read_las() with no option can be omitted
#' ans <- exec(rasterize(10, "zmax"), on = f)
#' # terra::plot(ans)
#'
#' # Perform a query and apply the pipeline on a subset
#' pipeline = reader_circles(273500, 5274500, 20) + rasterize(2, "zmax")
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#'
#' # Perform a query and apply the pipeline on a subset with 1 output files per query
#' ofile = paste0(tempdir(), "/*_chm.tif")
#' pipeline = reader_circles(273500, 5274500, 20) + rasterize(2, "zmax", ofile = ofile)
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#' @export
#' @md
reader = function(filter = "", select = "*", copc_depth = NULL, ...)
{
  p <- list(...)
  circle <- !is.null(p$xc)
  rectangle <-!is.null(p$xmin)

  if (circle) return(reader_circles(p$xc, p$yc, p$r, filter = filter, select = select, copc_depth = copc_depth, ...))
  if (rectangle) return(reader_rectangles(p$xmin, p$ymin, p$xmax, p$ymax, filter = filter, select = select, copc_depth = copc_depth,  ...))
  return(reader_coverage(filter = filter, select = select, copc_depth = copc_depth, ...))
}

#' @export
#' @rdname reader
reader_coverage = function(filter = "", select = "*", copc_depth = NULL, ...)
{
  validate_filter(filter, TRUE)
  if (is.null(copc_depth)) copc_depth = -1
  .APISTAGES$reader_coverage(filter, select, copc_depth)
}

#' @export
#' @rdname reader
reader_circles = function(xc, yc, r, filter = "", select = "*", copc_depth = NULL, ...)
{
  validate_filter(filter, TRUE)
  if (is.null(copc_depth)) copc_depth = -1
  .APISTAGES$reader_circles(xc, yc, r, filter, select, copc_depth)
}

#' @export
#' @rdname reader
reader_rectangles = function(xmin, ymin, xmax, ymax, filter = "", select = "*", copc_depth = NULL, ...)
{
  if (is.null(copc_depth)) copc_depth = -1
  .APISTAGES$reader_rectangles(xmin, ymin, xmax, ymax, filter, select, copc_depth)
}

#' Region growing
#'
#' Region growing for individual tree segmentation based on Dalponte and Coomes (2016) algorithm (see reference).
#' Note that this stage strictly performs segmentation, while the original method described in
#' the manuscript also performs pre- and post-processing tasks. Here, these tasks are expected to be
#' done by the user in separate functions.
#'
#' @param raster LASRalgoritm. A stage producing a raster.
#' @param seeds LASRalgoritm. A stage producing points used as seeds.
#' @param th_tree numeric. Threshold below which a pixel cannot be a tree. Default is 2.
#' @param th_seed numeric. Growing threshold 1. See reference in Dalponte et al. 2016. A pixel
#' is added to a region if its height is greater than the tree height multiplied by this value.
#' It should be between 0 and 1. Default is 0.45.
#' @param th_cr numeric. Growing threshold 2. See reference in Dalponte et al. 2016. A pixel
#' is added to a region if its height is greater than the current mean height of the region
#' multiplied by this value. It should be between 0 and 1. Default is 0.55.
#' @param max_cr numeric. Maximum value of the crown diameter of a detected tree **(in data units)**.
#' Default is 20. **BE CAREFUL** this algorithm exists in the `lidR` package and this parameter is in
#' pixels in `lidR`.
#' @template param-ofile
#'
#' @template return-raster
#'
#' @references
#' Dalponte, M. and Coomes, D. A. (2016), Tree-centric mapping of forest carbon density from
#' airborne laser scanning and hyperspectral data. Methods Ecol Evol, 7: 1236–1245. doi:10.1111/2041-210X.12575.
#'
#' @export
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package="lasR")
#'
#' reader <- reader(filter = keep_first())
#' chm <- rasterize(1, "max")
#' lmx <- local_maximum_raster(chm, 5)
#' tree <- region_growing(chm, lmx, max_cr = 10)
#' u <- exec(reader + chm + lmx + tree, on = f)
#'
#' # terra::plot(u$rasterize)
#' # plot(u$local_maximum, add = T, pch = 19, cex = 0.5)
#' # terra::plot(u$region_growing, col = rainbow(150))
#' # plot(u$local_maximum, add = T, pch = 19, cex = 0.5)
#' @md
region_growing = function(raster, seeds, th_tree = 2, th_seed = 0.45, th_cr = 0.55, max_cr = 20, ofile = temptif())
{
  rinfo = .APIOPERATIONS$get_stage_info(raster)
  sinfo = .APIOPERATIONS$get_stage_info(seeds)
  ofile = normalizePath(ofile, mustWork = FALSE)
  .APISTAGES$region_growing(rinfo[["uid"]], sinfo[["uid"]], th_tree, th_seed, th_cr, max_cr, ofile)
}

#' @export
#' @rdname add_attribute
remove_attribute = function(name) { .APISTAGES$remove_attribute(name) }

#' @export
#' @rdname add_attribute
remove_attributes = function(names) { .APISTAGES$remove_attributes(names) }

#' @export
#' @rdname add_rgb
remove_rgb = function() { .APISTAGES$remove_rgb() }

# ==== S =====

#' Set the CRS of the pipeline
#'
#' Assign a CRS in the pipeline. This stage **does not** reproject the data. It assigns a CRS. This
#' stage affects subsequent stages of the pipeline and thus should appear close to \link{reader}
#' to assign the correct CRS to all stages.
#'
#' @param x integer or string. EPSG code or WKT string understood by GDAL
#' @export
#' @md
#' @examples
#' # expected usage
#' hmax = rasterize(10, "max")
#' pipeline = reader() + set_crs(2949) + hmax
#'
#' # fancy usages are working as expected. The .tif file is written with a CRS, the .gpkg file with
#' # another CRS and the .las file with yet another CRS.
#' pipeline = set_crs(2044) + hmax + set_crs(2004) + local_maximum(5) + set_crs(2949) + write_las()
set_crs = function(x)
{
  if (is.numeric(x)) { return(.APISTAGES$set_crs_epsg(x)) }
  if (is.character(x)) { return(.APISTAGES$set_crs_wkt(x)) }
  stop("Invalid argument")
}

#' Sample the point cloud
#'
#' Sample the point cloud, keeping one random point per pixel or per voxel or perform a poisson sampling.
#' This stages modify the point cloud in the pipeline but do not produce any output. When used with a
#' 'filter' argument, only points that match the criteria are subsampled. Other point are kept as is
#' in the point cloud.
#'
#' @param res numeric. pixel/voxel resolution
#' @param distance numeric. Minimum distance between points for poisson sampling.
#' @param method string can be "random" to retain one random point or "min" or "max" to retain
#' the highest and lowest points respectively. For min and max users can use the argument `use_attribute`
#' to select the highest intensity or highest Z, or highest gpstime or any other attributes.
#' @param ... unused
#' @template param-attribute
#' @template param-filter
#' @template return-pointcloud
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#'
#' read <- reader()
#' vox <- sampling_voxel(5) # sample 1 random points per voxel
#' write <- write_las()
#' pipeline <- read + vox + write
#' ans = exec(pipeline, on = f)
#'
#' # Only ground points are poisson sampled. Other point are kept
#' vox <- sampling_poisson(10, filter = "Classification == 2")
#' write <- write_las()
#' pipeline <- read + vox + write
#' ans = exec(pipeline, on = f)
#' @export
#' @rdname sampling
sampling_voxel = function(res = 2, filter = "", ...)
{
  shuffle_size = .Machine$integer.max
  p = list(...)
  if (!is.null(p$shuffle_size)) shuffle_size = p$shuffle_size

  .APISTAGES$sampling_voxel(res, filter, "random", shuffle_size)
}

#' @export
#' @rdname sampling
sampling_pixel = function(res = 2, filter = "", method = "random", use_attribute = "Z", ...)
{
  method = match.arg(method, c("random", "max", "min"))

  shuffle_size = .Machine$integer.max
  p = list(...)
  if (!is.null(p$shuffle_size)) shuffle_size = p$shuffle_size

  .APISTAGES$sampling_pixel(res, filter, method, use_attribute, shuffle_size)
}

#' @export
#' @rdname sampling
sampling_poisson = function(distance = 2, filter = "", ...)
{
  shuffle_size = 0
  p = list(...)
  if (!is.null(p$shuffle_size)) shuffle_size = p$shuffle_size

  .APISTAGES$sampling_poisson(distance, filter, shuffle_size)
}


#' Stop the pipeline conditionally
#'
#' Stop the pipeline conditionally. The stages after a `stop_if` stage are skipped if the condition is
#' met. This allows to process a subset of the dataset of to skip some stages conditionally. This DOES
#' NOT stop the computation. In only breaks the pipeline for the current file/chunk currently processed.
#' (see exemple)
#'
#' @param xmin,ymin,xmax,ymax numeric. bounding box
#'
#' @examples
#' # Collection of 4 files
#' f <- system.file("extdata", "bcts/", package="lasR")
#'
#' # This bounding box encompasses only one of the four files
#' stopif = stop_if_outside(884800, 620000, 885400, 629200)
#'
#' read = reader()
#' hll = hulls()
#' tri = triangulate(filter = keep_ground())
#' dtm = rasterize(1, tri)
#'
#' # reads the 4 files but 'tri' and 'dtm' are computed only for one file because stopif
#' # allows to escape the pipeline outside the bounding box
#' pipeline = read + hll + stopif + tri + dtm
#' ans1 <- exec(pipeline, on = f)
#' plot(ans1$hulls$geom, axes = TRUE)
#' terra::plot(ans1$rasterize, add = TRUE)
#'
#' # stopif can be applied before read. Only one file will actually be read and processed
#' pipeline = stopif + read + hll + tri + dtm
#' ans2 <- exec(pipeline, on = f)
#' plot(ans2$hulls$geom, axes = TRUE)
#' terra::plot(ans1$rasterize, add = TRUE, legend = FALSE)
#' @export
stop_if_outside = function(xmin, ymin, xmax, ymax) { .APISTAGES$stop_if_outside(xmin, ymin, xmax, ymax) }


stop_if_chunk_id_below = function(index) { .APISTAGES$stop_if_chunk_id_below(index) }

#' Sort points in the point cloud
#'
#' This stage sorts points spatially. A grid of 50 meters is applied, and points are sorted within
#' each cell of the grid. This increases data locality, speeds up spatial queries, but may slightly
#' increases the final size of the files when compressed in LAZ format compared to the optimal compression.
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' exec(sort_points(), on = f)
#' @export
sort_points = function() { .APISTAGES$sort_points(TRUE) }

#' Summary
#'
#' Summarize the dataset by counting the number of points, first returns and other metrics for the **entire point cloud**.
#' It also produces an histogram of Z and Intensity attributes for the **entiere point cloud**.
#' It can also compute some metrics **for each file or chunk** with the same metric engine than \link{rasterize}.
#' This stage does not modify the point cloud. It produces a summary as a `list`.
#'
#' @param zwbin,iwbin numeric. Width of the bins for the histograms of Z and Intensity.
#' @param metrics Character vector. "min", "max" and "count" are accepted as well
#' as many others (see \link{metric_engine}). If `NULL` nothing is computed. If something is provided these
#' metrics are computed for each chunk loaded. A chunk might be a file but may also be a plot (see examples).
#' @template param-filter
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader()
#' pipeline <- read + summarise()
#' ans <- exec(pipeline, on = f)
#' ans
#'
#' # Compute metrics for each plot
#' read = reader_circles(c(273400, 273500), c(5274450, 5274550), 11.28)
#' metrics = summarise(metrics = c("z_mean", "z_p95", "i_median", "count"))
#' pipeline = read + metrics
#' ans = exec(pipeline, on = f)
#' ans$metrics
#' @export
#' @md
summarise = function(zwbin = 2, iwbin = 50, metrics = NULL, filter = "")
{
  if (is.null(metrics)) metrics = character()
  return(.APISTAGES$summarise(zwbin, iwbin, metrics, filter))
}

# ===== T =====

#' Delaunay triangulation
#'
#' 2.5D Delaunay triangulation. Can be used to build a DTM, a CHM, normalize a point cloud, or any other
#' application. This stage is typically used as an intermediate process without an output file.
#' This stage does not modify the point cloud.
#'
#' @param max_edge numeric. Maximum edge length of a triangle in the Delaunay triangulation. If a
#' triangle has an edge length greater than this value, it will be removed. If max_edge = 0, no trimming
#' is done (see examples).
#' @template param-attribute
#' @template param-filter
#' @template param-ofile
#'
#' @template return-vector
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader()
#' tri1 <- triangulate(25, filter = keep_ground(), ofile = tempgpkg())
#' tri2 <- triangulate(ofile = tempgpkg())
#' pipeline <- read + tri1 + tri2
#' ans <- exec(pipeline, on = f)
#' #plot(ans[[1]])
#' #plot(ans[[2]])
#' @export
triangulate = function(max_edge = 0, filter = "", ofile = "", use_attribute = "Z")
{
  .APISTAGES$triangulate(max_edge, filter, ofile, use_attribute)
}

#' Transform a Point Cloud Using Another Stage
#'
#' This stage uses another stage to modify the point cloud in the pipeline. When used with a Delaunay
#' triangulation or a raster, it performs an operation to modify the Z coordinate of the point cloud.
#' The interpolation method is linear in the triangle mesh and bilinear in the raster. This can typically
#' be used to build a normalization stage.\cr
#' When used with a 4x4 Rotation-Translation Matrix, it multiplies the coordinates of the points to apply
#' the rigid transformation described by the matrix. This stage modifies the point cloud in the pipeline
#' but does not produce any output.
#'
#' @section RTM Matriz:
#' **Warning:** `lasR` uses bounding boxes oriented along the XY axes of each processed chunk to manage
#' data location and the buffer properly. Transforming the point cloud with a rotation matrix affects
#' its bounding box and how `lasR` handles the buffer. When used with a matrix that has a rotational
#' component, it is not safe to add stages after the transformation unless the user is certain that
#' there is no buffer involved.
#'
#' @param stage A stage that produces a triangulation, raster, or Rotation-Translation Matrix (RTM),
#' sometimes also referred to as an "Affine Transformation Matrix". Can also be a 4x4 RTM matrix.
#' @param operator A string. '-' and '+' are supported (only with a triangulation or a raster).
#' @param store_in_attribute A string. Use an extra byte attribute to store the result (only with
#' a triangulation or a raster).
#' @param bilinear bool. If the stage is a raster stage, the Z values are interpolated with a bilinear
#' interpolation. FALSE to desactivate it.
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#'
#' # with a triangulation
#' mesh  <- triangulate(filter = keep_ground())
#' trans <- transform_with(mesh)
#' pipeline <- mesh + trans + write_las()
#' ans <- exec(pipeline, on = f)
#'
#' # with a matrix
#' a = 20 * pi / 180
#' m <- matrix(c(
#'   cos(a), -sin(a), 0, 1000,
#'   sin(a), cos(a), 0, 0,
#'   0, 0, 1, 0,
#'   0, 0, 0, 1), nrow = 4, byrow = TRUE)
#'
#' pipeline = transform_with(m) + write_las()
#' exec(pipeline, on = f)
#' @seealso
#' \link{triangulate}
#' \link{write_las}
#' @export
#' @md
transform_with = function(stage, operator = "-", store_in_attribute = "", bilinear = TRUE)
{
  use_matrix = FALSE
  if (is.matrix(stage))
  {
    use_matrix = TRUE
    stage <- load_matrix(stage)
  }

  if (!methods::is(stage, "PipelinePtr"))
    stop("The stage stage must be a 'PipelinePtr'")

  s <- .APIOPERATIONS$get_stage_info(stage)

  # Valid stage are all raster stages or triangulate
  if (s$name != "triangulate" && !s$raster && !s$matrix)
      stop("The stage must be a triangulation or a raster stage or a matrix stage.")

  ans = .APISTAGES$transform_with(s[["uid"]], operator, store_in_attribute, bilinear)

  if (use_matrix)
    ans = stage + ans

  return(ans)
}

# ===== W ====

#' Write point clouds
#'
#' Write the point cloud in LAS or LAZ or PCD files at any step of the pipeline (typically at the end).
#' Unlike other stages, the output won't be written into a single large file but in multiple tiled files
#' corresponding to the original collection of files.
#'
#' `write_las` can write a COPC LAZ file simply by naming the output file with the ".copc.laz" extension.
#' However, users must be cautious. Writing COPC is not optimized for memory usage and requires two copies
#' of the point cloud in memory to ensure proper sorting and writing. If the user cannot afford to keep
#' two copies of the point cloud in RAM, they should use a more specialized writer such as Untwine (PDAL)
#' or lascopcindex (LAStools).\cr
#' `write_copc` is a wrapper around `write_las`, with a few extra arguments to control the COPC format.\cr
#' `write_pcd` cannot merge multiple files into one bigger file yet. It cannot write a subset of the file
#' either yet.
#'
#' @param ofile character. Output file names. The string must contain a wildcard * so the wildcard can
#' be replaced by the name of the original tile and preserve the tiling pattern. If the wildcard
#' is omitted, everything will be written into a single file. This may be the desired behavior in some
#' circumstances, e.g., to merge some files.
#' @param keep_buffer bool. The buffer is removed to write file but it can be preserved.
#' @param binary boolean. Write binary or ascii PCD files.
#' @template param-filter
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader()
#' tri  <- triangulate(filter = keep_ground())
#' normalize <- tri + transform_with(tri)
#' pipeline <- read + normalize + write_las(paste0(tempdir(), "/*_norm.las"))
#' exec(pipeline, on = f)
#' @export
#' @md
#' @rdname write
write_las = function(ofile = paste0(tempdir(), "/*.las"), filter = "", keep_buffer = FALSE)
{
  validate_filter(filter)
  return(.APISTAGES$write_las(ofile, filter, keep_buffer))
}

#' @export
#' @rdname write
#' @param max_depth integer. Maximum depth of the hierarchy. Default is NA meaning that is auto computes
#' @param density character. Can be 'sparse', 'normal' or 'dense'. It controls the point density per octant.
#' With 'sparce' each Octree octant is subdivided into 64 x 64 x 64 cells which mean that the density of point
#' is light. Normal is 128, dense is 256.
write_copc = function(ofile = paste0(tempdir(), "/*.copc.laz"), filter = "", keep_buffer = FALSE, max_depth = NA, density = "dense")
{
  ofile = normalizePath(ofile, mustWork = FALSE)
  if (is.na(max_depth)) max_depth = -1
  .APISTAGES$write_copc(ofile, filter, keep_buffer, max_depth, density)
}

#' @export
#' @rdname write
write_pcd = function(ofile = paste0(tempdir(), "/*.pcd"), binary = TRUE)
{
  ofile = normalizePath(ofile, mustWork = FALSE)
  .APISTAGES$write_pcd(ofile, binary)
}

#' Write a Virtual Point Cloud
#'
#' Borrowing the concept of virtual rasters from GDAL, the VPC file format references other point
#' cloud files in virtual point cloud (VPC)
#'
#' @param ofile character. The file path with extension .vpc where to write the virtual point cloud file
#' @param absolute_path boolean. The absolute path to the files is stored in the tile index file.
#' @param use_gpstime logical. To fill the datetime attribute in the VPC file, it uses the year and
#' day of year recorded in the header. These attributes are usually NOT relevant. They are often zeroed
#' and the official signification of these attributes corresponds to the creation of the LAS file.
#' There is no guarantee that this date corresponds to the acquisition date. If `use_gpstime = TRUE`,
#' it will use the gpstime of **the first point** recorded in each file to compute the day and year of
#' acquisition. This works only if the GPS time is recorded as Adjusted Standard GPS Time and not with GPS
#' Week Time.
#' @references
#' \url{https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/}\cr
#' \url{https://github.com/PDAL/wrench/blob/main/vpc-spec.md}
#' @examples
#' \dontrun{
#' pipeline = write_vpc("folder/dataset.vpc")
#' exec(pipeline, on = "folder")
#' }
#' @md
#' @export
write_vpc = function(ofile, absolute_path = FALSE, use_gpstime = FALSE)
{
  ofile = normalizePath(ofile, mustWork = FALSE)
  .APISTAGES$write_vpc(ofile, absolute_path, use_gpstime)
}

#' Write spatial indexing .lax files
#'
#' Creates a .lax file for each `.las` or `.laz` file of the processed datase. A .lax file contains spatial
#' indexing information. Spatial indexing drastically speeds up tile buffering and spatial queries.
#' In lasR, it is mandatory to have spatially indexed point clouds, either using .lax files or .copc.laz
#' files. If the processed file collection is not spatially indexed, a `write_lax()` file will automatically
#' be added at the beginning of the pipeline (see Details).
#'
#' When this stage is added automatically by `lasR`, it is placed at the beginning of the pipeline, and las/laz
#' files are indexed **on-the-fly** when they are used. The advantage is that users do not need to do anything;
#' it works transparently and does not delay the processing. The drawback is that, under this condition,
#' the stage cannot be run in parallel. When this stage is explicitly added by the users, it can be
#' placed anywhere in the pipeline but will always be executed first before anything else. All the
#' files will be indexed first in parallel, and then the actual processing will start. To avoid overthinking
#' about how it works, it is best and simpler to run `exec(write_lax(), on = files)` on the non indexed
#' point cloud before doing anything with the point cloud.
#'
#' @param embedded boolean. A .lax file is an auxiliary file that accompanies its corresponding las
#' or laz file. A .lax file can also be embedded within a laz file to produce a single file.
#' @param overwrite boolean. This stage does not create a new spatial index if the corresponding point cloud
#' already has a spatial index. If TRUE, it forces the creation of a new one. `copc.laz` files are never reindexed
#' with `lax` files.
#' @export
#' @md
#' @examples
#' \dontrun{
#' exec(write_lax(), on = files)
#' }
write_lax = function(embedded = FALSE, overwrite = FALSE)
{
  .APISTAGES$write_lax(embedded, overwrite)
}

LASATTRIBUTES <- c("X", "Y", "Z", "Intensity",
                   "ReturnNumber", "NumberOfReturns",
                   "ScanDirectionFlag", "EdgeOfFlightline",
                   "Classification",
                   "Synthetic", "Keypoint",
                   "Withheld", "Overlap",
                   "ScanAngle",
                   "ScannerChannel", "NIR",
                   "UserData", "gpstime", "PointSourceID",
                   "R", "G", "B")


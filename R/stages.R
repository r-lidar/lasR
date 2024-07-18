# ===== A =====

#' Add attributes to a LAS file
#'
#' According to the \href{https://www.asprs.org/a/society/committees/standards/LAS_1_4_r13.pdf}{LAS specifications},
#' a LAS file contains a core of defined attributes, such as XYZ coordinates, intensity, return number,
#' and so on, for each point. It is possible to add supplementary attributes. This stages adds an
#' extra bytes attribute to the points. Values are zeroed: the underlying point cloud is edited to support
#' a new extrabyte attribute. This new attribute can be populated later in another stage
#'
#' @param name character. The name of the extra bytes attribute to add to the file.
#' @param description character. A short description of the extra bytes attribute to add to the file (32 characters).
#' @param data_type character. The data type of the extra bytes attribute. Can be "uchar", "char", "ushort",
#' "short", "uint", "int", "uint64", "int64", "float", "double".
#' @param scale,offset numeric. The scale and offset of the data. See LAS specification.
#' @template return-pointcloud
#' @export
#'
#' @examples
#' f <- system.file("extdata", "Example.las", package = "lasR")
#' fun <- function(data) { data$RAND <- runif(nrow(data), 0, 100); return(data) }
#' pipeline <- reader_las() +
#'   add_extrabytes("float", "RAND", "Random numbers") +
#'   callback(fun, expose = "xyz")
#' exec(pipeline, on = f)
add_extrabytes = function(data_type, name, description, scale = 1, offset = 0)
{
  types <- c("uchar", "char", "ushort", "short", "uint", "int", "uint64", "int64", "float", "double")
  data_type <- match.arg(data_type, types)
  #data_type <- which(data_type == types)

  ans <- list(algoname = "add_extrabytes", data_type = data_type, name = name, description = description, scale = scale, offset = offset)
  set_lasr_class(ans)
}

#' Add RGB attributes to a LAS file
#'
#' Modifies the LAS format to convert into a format with RGB attributes. Values are zeroed: the underlying
#' point cloud is edited to be transformed in a format that supports RGB. RGB can be populated later
#' in another stage. If the point cloud already has RGB, nothing happens, RGB values are preserved.
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Example.las", package="lasR")
#'
#' pipeline <- add_rgb() + write_las()
#' exec(pipeline, on = f)
#' @export
add_rgb = function()
{
  ans <- list(algoname = "add_rgb")
  set_lasr_class(ans)
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

  ans <- list(algoname = "aggregate", res = res_raster, nmetrics = nmetrics, window = res_window, call = call, filter = filter, output = ofile, env = env)
  set_lasr_class(ans, raster = TRUE)
}

# ===== B =====

build_catalog = function(files, with)
{
  ans <- list(algoname = "build_catalog",  files = files, buffer = with$buffer, noprocess = with$noprocess)
  set_lasr_class(ans)
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
#' r - return number, c - classification, s - synthetic flag, k - keypoint flag, w - withheld flag,
#' o - overlap flag (format 6+), u - user data, p - point source ID, e - edge of flight line flag,
#' d - direction of scan flag, R - red channel of RGB color, G - green channel of RGB color,
#' B - blue channel of RGB color, N - near-infrared channel, C - scanner channel (format 6+)
#' Also numbers from 1 to 9 for the extra bytes data numbers 1 to 9. 'E' enables all extra bytes to be
#' loaded.  '*' is the wildcard that enables everything to be exposed from the LAS file.
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
#'   read <- reader_las()
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
#' read <- reader_las()
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

  ans <- list(algoname = "callback", fun = fun, expose = expose, drop_buffer = drop_buffer, no_las_update = no_las_update, args = fargs)
  set_lasr_class(ans)
}

#' Classify noise points
#'
#' Classify points using Isolated Voxel Filter. The stage identifies points that have only a few other
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
classify_with_ivf = function(res = 5, n = 6L, class = 18L)
{
  ans <- list(algoname = "classify_with_ivf", res = res, n = n, class = class)
  set_lasr_class(ans)
}

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
classify_with_csf = function(slope_smooth = FALSE, class_threshold = 0.5, cloth_resolution = 0.5, rigidness = 1L, iterations = 500L, time_step = 0.65, class = 2L)
{
  ans <- list(algoname = "classify_with_csf", slope_smooth = slope_smooth, class_threshold = class_threshold, cloth_resolution = cloth_resolution, rigidness = rigidness, iterations = iterations, time_step = time_step, class = class)
  set_lasr_class(ans)
}

# ===== G =====

#' Compute pointwise geometry features
#'
#' Compute pointwise geometry features based on local neighborhood. Each feature is added into an
#' extrabyte attribute. The names of the extrabytes attributes (if recorded) are `coeff00`, `coeff01`,
#' `coeff02` and so on, `lambda1`, `lambda2`, `lambda3`, `anisotropy`, `planarity`, `sphericity`, `linearity`,
#' `omnivariance`, `curvature`, `eigensum`, `angle`, `normalX`, `normalY`, `normalZ` (recorded in this order).
#' There is a total of 23 attributes that can be added. It is strongly discouraged to use them all.
#' All the features are recorded with single precision floating points yet computing them all will triple
#' the size of the point cloud. This stage modifies the point cloud in the pipeline but does not produce
#' any output.
#'
#' @param k,r integer and numeric respectively for k-nearest neighbours and radius of the neighborhood
#' sphere. If k is given and r is missing, computes with the knn, if r is given and k is missing
#' computes with a sphere neighborhood, if k and r are given computes with the knn and a limit on the
#' search distance.
#' @param features String. Geometric feature to export. Each feature is added into an extrabyte
#' attribute. Use 'C' for the 9 principal component coefficients, 'E' for the 3 eigenvalues of the
#' covariance matrix, 'a' for anisotropy, 'p' for planarity, 's' for sphericity, 'l' for linearity,
#' 'o' for omnivariance, 'c' for curvature, 'e' for the sum of eigenvalues, 'i' for the angle
#' (inclination in degrees relative to the azimuth), and 'n' for the 3 components of the normal vector.
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
  if (!missing(k) && missing(r))  { r <- 0 }   # knn
  if (!missing(r) && missing(k)) { k <- 0 }   # radius

  ans <- list(algoname = "svd", k = k, r = r, features = features)
  set_lasr_class(ans)
}


# ===== D =====

#' Filter and delete points
#'
#' Remove some points from the point cloud. This stage modifies the point cloud in the pipeline
#' but does not produce any output.
#'
#' @template param-filter
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Megaplot.las", package="lasR")
#' read <- reader_las()
#' filter <- delete_points(keep_z_above(4))
#'
#' pipeline <- read + summarise() + filter + summarise()
#' exec(pipeline, on = f)
#' @export
delete_points = function(filter = "")
{
  stopifnot(filter != "")
  ans = list(algoname = "filter", filter = filter)
  set_lasr_class(ans)
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
#' read <- reader_las()
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
    mesh = get_stage(mesh)
    if (mesh$algoname != "triangulate") stop("the stage must be 'triangulate'")  # nocov
    ans <- list(algoname = "hulls", connect = mesh[["uid"]], output = ofile)
  }
  else
  {
    ans <- list(algoname = "hulls", output = ofile)
  }

  set_lasr_class(ans, vector = TRUE)
}

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
  ans <- list(algoname = "load_raster", file = file, band = band)
  set_lasr_class(ans, raster = TRUE)
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
#' @param use_attribute character. By default the local maximum is performed on the coordinate Z. Can also be
#' the name of an extra bytes attribute such as 'HAG' if it exists. Can also be 'Intensity' but there is
#' probably no use case for that one.
#' @param raster LASRalgorithm. A stage that produces a raster.
#' @param record_attributes The coordinates XYZ of points corresponding to the local maxima are recorded.
#' It is also possible to record the attributes of theses points such as the intensity, return number, scan
#' angle and so on.
#'
#' @template param-filter
#' @template param-ofile
#'
#' @template return-vector
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package = "lasR")
#' read <- reader_las()
#' lmf <- local_maximum(5)
#' ans <- exec(read + lmf, on = f)
#' ans
#'
#' chm <- rasterize(1, "max")
#' lmf <- local_maximum_raster(chm, 5)
#' ans <- exec(read + chm + lmf, on = f)
#' # terra::plot(ans$rasterize)
#' # plot(ans$local_maximum, add = T, pch = 19)
#' @export
#' @md
local_maximum = function(ws, min_height = 2, filter = "", ofile = tempgpkg(), use_attribute = "Z", record_attributes = FALSE)
{
  ans <- list(algoname = "local_maximum", ws = ws, min_height = min_height, filter = filter, output = ofile, use_attribute = use_attribute, record_attributes = record_attributes)
  set_lasr_class(ans, vector = TRUE)
}

#' @export
#' @rdname local_maximum
local_maximum_raster = function(raster, ws, min_height = 2, filter = "", ofile = tempgpkg())
{
  raster = get_stage(raster)

  if (!methods::is(raster, "LASRraster"))  stop("the stage must be a raster stage")

  ans <- list(algoname = "local_maximum", connect = raster[["uid"]], ws = ws, min_height = min_height, filter = filter, output = ofile)
  set_lasr_class(ans, vector = TRUE)
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
#' read <- reader_las()
#' lmf <- local_maximum(5, ofile = "")
#' nnm = lasR:::neighborhood_metrics(lmf, k = 10, metrics = c("i_mean", "count"))
#' ans <- exec(read + lmf + nnm, on = f)
#' ans
#' @noRd
neighborhood_metrics = function(neighborhood, metrics, k = 10, r = 0, ofile = tempgpkg())
{
  nn = get_stage(neighborhood)
  if (nn$algoname != "local_maximum") stop("the stage must be a local_maximum stage")

  ans <- list(algoname = "neighborhood_metrics", connect = nn[["uid"]], k = k, r = r, metrics = metrics, output = ofile)
  set_lasr_class(ans)
}

nothing = function(read = FALSE, stream = FALSE, loop = FALSE)
{
  ans <- list(algoname = "nothing", read = read, stream = stream, loop = loop)
  set_lasr_class(ans)
}

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
#' reader <- reader_las(filter = keep_first())
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
  raster = get_stage(raster)
  if (!methods::is(raster, "LASRraster")) stop("'raster' must be a raster stage")  # nocov

  ans <- list(algoname = "pit_fill", connect = raster[["uid"]],  lap_size = lap_size, thr_lap = thr_lap, thr_spk = thr_spk, med_size = med_size, dil_radius = dil_radius, output = ofile)
  set_lasr_class(ans, raster = TRUE)
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
#' \cr
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
#' read <- reader_las()
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

  res_raster  <- res[1]
  res_window <- res[1]
  if (length(res) > 1L)
    res_window <- res[2]

  if (class == "call")
  {
    env <- new.env(parent = parent.frame())
    return(aggregate_q(res, substitute(operators), filter, ofile, env))
  }

  if (methods::is(operators, "LASRpipeline") || methods::is(operators, "LASRalgorithm"))
  {
    operators = get_stage(operators)
    ans <- list(algoname = "rasterize", res = res_raster, connect = operators[["uid"]], filter = filter, output = ofile)
  }
  else if (is.character(operators))
  {
    # backward compatibility
    former_operators <- c("max", "min", "count", "zmax", "zmin", "zmean", "zmedian", "zsd", "zcv", "zabove", "zp", "imax", "imin", "imean", "imedian", "isd", "icv", "iabove", "ip")
    new_operators <- c("max", "min", "count", "z_max", "z_min", "z_mean", "z_median", "z_sd", "z_cv", "z_above", "z_p", "i_max", "i_min", "i_mean", "i_median", "i_sd", "i_cv", "i_above", "i_p")
    replace_names <- function(x, old_names, new_names)
    {
      for (i in seq_along(old_names))
      {
        x <- gsub(old_names[i], new_names[i], x)
      }
      return(x)
    }
    operators <- replace_names(operators, former_operators, new_operators)

    ans <- list(algoname = "rasterize", res = res_raster, window = res_window, method = operators, filter = filter, output = ofile, default_value = default_value)
  }
  else
  {
    stop("Invalid operators") # nocov
  }
  set_lasr_class(ans, raster = TRUE)
}

#' Initialize the pipeline
#'
#' This is the first stage that must be called in each pipeline. The stage does nothing and returns
#' nothing if it is not associated to another processing stage.
#' It only initializes the pipeline. `reader_las()` is the main function that dispatches into to other
#' functions. `reader_las_coverage()` processes the entire point cloud. `reader_las_circles()` and
#' `reader_las_rectangles()` read and process only some selected regions of interest. If the chosen
#' reader has no options i.e. using `reader_las()` it can be omitted.
#'
#' @template param-filter
#' @param xc,yc,r numeric. Circle centres and radius or radii.
#' @param xmin,ymin,xmax,ymax numeric. Coordinates of the rectangles
#' @param ... passed to other readers
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#'
#' pipeline <- reader_las() + rasterize(10, "zmax")
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#'
#' pipeline <- reader_las(filter = keep_z_above(1.3)) + rasterize(10, "zmean")
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#'
#' # read_las() with no option can be omitted
#' ans <- exec(rasterize(10, "zmax"), on = f)
#' # terra::plot(ans)
#'
#' # Perform a query and apply the pipeline on a subset
#' pipeline = reader_las_circles(273500, 5274500, 20) + rasterize(2, "zmax")
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#'
#' # Perform a query and apply the pipeline on a subset with 1 output files per query
#' ofile = paste0(tempdir(), "/*_chm.tif")
#' pipeline = reader_las_circles(273500, 5274500, 20) + rasterize(2, "zmax", ofile = ofile)
#' ans <- exec(pipeline, on = f)
#' # terra::plot(ans)
#' @export
#' @md
reader_las = function(filter = "", ...)
{
  p <- list(...)
  circle <- !is.null(p$xc)
  rectangle <-!is.null(p$xmin)

  if (circle) return(reader_las_circles(p$xc, p$yc, p$r, filter = filter, ...))
  if (rectangle) return(reader_las_rectangles(p$xmin, p$ymin, p$xmax, p$ymax, filter = filter, ...))
  return(reader_las_coverage(filter = filter, ...))
}

#' @export
#' @rdname reader_las
reader_las_coverage = function(filter = "", ...)
{
  ans <- list(algoname = "reader_las", filter = filter)
  set_lasr_class(ans)
}

#' @export
#' @rdname reader_las
reader_las_circles = function(xc, yc, r, filter = "", ...)
{
  stopifnot(length(xc) == length(yc))
  if (length(r) == 1) r <- rep(r, length(xc))
  if (length(r) > 1) stopifnot(length(xc) == length(r))

  ans <- reader_las_coverage(filter, ...)
  ans[[1]]$xcenter <- xc
  ans[[1]]$ycenter <- yc
  ans[[1]]$radius <- r
  ans
}

#' @export
#' @rdname reader_las
reader_las_rectangles = function(xmin, ymin, xmax, ymax, filter = "", ...)
{
  stopifnot(length(xmin) == length(ymin), length(xmin) == length(xmax), length(xmin) == length(ymax))

  ans <- reader_las_coverage(filter, ...)
  ans[[1]]$xmin <- xmin
  ans[[1]]$ymin <- ymin
  ans[[1]]$xmax <- xmax
  ans[[1]]$ymax <- ymax
  ans
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
#' reader <- reader_las(filter = keep_first())
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
  raster <- get_stage(raster)
  seeds <- get_stage(seeds)
  ans <- list(algoname = "region_growing", connect1 = seeds[["uid"]], connect2 = raster[["uid"]], th_tree = th_tree, th_seed = th_seed, th_cr = th_cr, max_cr = max_cr, output = ofile)
  set_lasr_class(ans, raster = TRUE)
}

# ==== S =====

#' Set the CRS of the pipeline
#'
#' Assign a CRS in the pipeline. This stage **does not** reproject the data. It assigns a CRS. This
#' stage affects subsequent stages of the pipeline and thus should appear close to \link{reader_las}
#' to assign the correct CRS to all stages.
#'
#' @param x integer or string. EPSG code or WKT string understood by GDAL
#' @export
#' @md
#' @examples
#' # expected usage
#' hmax = rasterize(10, "max")
#' pipeline = reader_las() + set_crs(2949) + hmax
#'
#' # fancy usages are working as expected. The .tif file is written with a CRS, the .gpkg file with
#' # another CRS and the .las file with yet another CRS.
#' pipeline = set_crs(2044) + hmax + set_crs(2004) + local_maximum(5) + set_crs(2949) + write_las()
set_crs = function(x)
{
  epsg = 0
  wkt = ""
  if (is.numeric(x)) { epsg = x }
  if (is.character(x)) { wkt = x }
  ans <- list(algoname = "set_crs", epsg = epsg, wkt = wkt)
  set_lasr_class(ans)
}

#' Sample the point cloud
#'
#' Sample the point cloud, keeping one random point per pixel or per voxel or perform a poisson sampling.
#' This stages modify the point cloud in the pipeline but do not produce any output.
#'
#' @param res numeric. pixel/voxel resolution
#' @param distance numeric. Minimum distance between points for poisson sampling.
#' @param ... unused
#' @template param-filter
#' @template return-pointcloud
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader_las()
#' vox <- sampling_voxel(5)
#' write <- write_las()
#' pipeline <- read + vox + write
#' exec(pipeline, on = f)
#' @export
#' @rdname sampling
sampling_voxel = function(res = 2, filter = "", ...)
{
  shuffle_size = .Machine$integer.max
  p = list(...)
  if (!is.null(p$shuffle_size)) shuffle_size = p$shuffle_size

  ans <- list(algoname = "sampling_voxel", res = res, filter = filter, shuffle_size = shuffle_size)
  set_lasr_class(ans)
}

#' @export
#' @rdname sampling
sampling_pixel = function(res = 2, filter = "", ...)
{
  shuffle_size = .Machine$integer.max
  p = list(...)
  if (!is.null(p$shuffle_size)) shuffle_size = p$shuffle_size

  ans <- list(algoname = "sampling_pixel", res = res, filter = filter, shuffle_size = shuffle_size)
  set_lasr_class(ans)
}

#' @export
#' @rdname sampling
sampling_poisson = function(distance = 2, filter = "", ...)
{
  shuffle_size = 1000
  p = list(...)
  if (!is.null(p$shuffle_size)) shuffle_size = p$shuffle_size

  ans <- list(algoname = "sampling_poisson", distance = distance, filter = filter, shuffle_size = shuffle_size)
  set_lasr_class(ans)
}


#' Stop the pipeline if a conditionally
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
#' read = reader_las()
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
stop_if_outside = function(xmin, ymin, xmax, ymax)
{
  ans <- list(algoname = "stop_if", condition = "outside_bbox", xmin = xmin, ymin = ymin, xmax = xmax, ymax = ymax)
  set_lasr_class(ans)
}

#' Sort points in the point cloud
#'
#' This stage sorts the points by scanner channel, GPStime, and return number in order to maximize LAZ
#' compression. An optional second sorting step can be added to also sort points spatially. In this case,
#' a grid of 50 meters is applied, and points are sorted by scanner channel, GPSTime, and return number
#' within each cell of the grid. This increases data locality, speeds up spatial queries, but may slightly
#' increases the final size of the files when compressed in LAZ format compared to the optimal compression.
#'
#' @param spatial Boolean indicating whether to add a spatial sorting stage.
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' exec(sort_points(), on = f)
#' @export
sort_points = function(spatial = TRUE)
{
  ans <- list(algoname = "sort", spatial = spatial)
  set_lasr_class(ans)
}

stop_if_chunk_id_below = function(index)
{
  index <- as.integer(index)
  ans <- list(algoname = "stop_if", condition = "chunk_id_below", index = index)
  set_lasr_class(ans)
}

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
#' read <- reader_las()
#' pipeline <- read + summarise()
#' ans <- exec(pipeline, on = f)
#' ans
#'
#' # Compute metrics for each plot
#' read = reader_las_circles(c(273400, 273500), c(5274450, 5274550), 11.28)
#' metrics = summarise(metrics = c("z_mean", "z_p95", "i_median", "count"))
#' pipeline = read + metrics
#' ans = exec(pipeline, on = f)
#' ans$metrics
#' @export
#' @md
summarise = function(zwbin = 2, iwbin = 50, metrics = NULL, filter = "")
{
  ans <- list(algoname = "summarise", filter = filter, zwbin = zwbin, iwbin = iwbin, metrics = metrics)
  set_lasr_class(ans)
}

# ===== T =====

#' Delaunay triangulation
#'
#' Delaunay triangulation. Can be used to build a DTM, a CHM, normalize a point cloud, or any other
#' application. This stage is typically used as an intermediate process without an output file.
#' This stage does not modify the point cloud.
#'
#' @param max_edge numeric. Maximum edge length of a triangle in the Delaunay triangulation. If a
#' triangle has an edge length greater than this value, it will be removed. If max_edge = 0, no trimming
#' is done (see examples).
#' @param use_attribute character. By default the triangulation is performed on the coordinate Z. Can also be
#' the name of an extra bytes attribute such as 'HAG' if it exists. Can also be 'Intensity'.
#'
#' @template param-filter
#' @template param-ofile
#'
#' @template return-vector
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader_las()
#' tri1 <- triangulate(25, filter = keep_ground(), ofile = tempgpkg())
#' filter <- "-keep_last -keep_random_fraction 0.1"
#' tri2 <- triangulate(filter = filter, ofile = tempgpkg())
#' pipeline <- read + tri1 + tri2
#' ans <- exec(pipeline, on = f)
#' #plot(ans[[1]])
#' #plot(ans[[2]])
#' @export
triangulate = function(max_edge = 0, filter = "", ofile = "", use_attribute = "Z")
{
  ans <- list(algoname = "triangulate", max_edge = max_edge, filter = filter, output = ofile, use_attribute = use_attribute)
  set_lasr_class(ans, vector = TRUE)
}

#' Transform a point cloud using another stage
#'
#' This stage uses another stage that produced a Delaunay triangulation or a raster and performs an
#' operation to modify the point cloud. This can typically be used to build a normalization stage
#' This stage modifies the point cloud in the pipeline but does not produce any output.
#'
#' @param stage LASRpipeline. A stage that produces a triangulation or a raster.
#' @param operator string. '-' and '+' are supported.
#' @param store_in_attribute string. Use an extra bytes attribute to store the result.
#'
#' @template return-pointcloud
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#'
#' # There is a normalize pipeline in lasR but let's create one almost equivalent
#' mesh  <- triangulate(filter = keep_ground())
#' trans <- transform_with(mesh)
#' pipeline <- mesh + trans + write_las()
#' ans <- exec(pipeline, on = f)
#'
#' @seealso
#' \link{triangulate}
#' \link{write_las}
#' @export
transform_with = function(stage, operator = "-", store_in_attribute = "")
{
  stage = get_stage(stage)

  # Valid stage are all raster stages or triangulate
  if (stage$algoname != "triangulate" && !methods::is(stage, "LASRraster"))
      stop("the stage must be a triangulation or a raster stage")

  ans <- list(algoname = "transform_with", connect = stage[["uid"]], operator = operator, store_in_attribute = store_in_attribute)
  set_lasr_class(ans)
}

# ===== W ====

#' Write LAS or LAZ files
#'
#' Write a LAS or LAZ file at any step of the pipeline (typically at the end). Unlike other stages,
#' the output won't be written into a single large file but in multiple tiled files corresponding
#' to the original collection of files.
#'
#' @param ofile character. Output file names. The string must contain a wildcard * so the wildcard can
#' be replaced by the name of the original tile and preserve the tiling pattern. If the wildcard
#' is omitted, everything will be written into a single file. This may be the desired behavior in some
#' circumstances, e.g., to merge some files.
#' @param keep_buffer bool. The buffer is removed to write file but it can be preserved.
#' @template param-filter
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader_las()
#' tri  <- triangulate(filter = keep_ground())
#' normalize <- tri + transform_with(tri)
#' pipeline <- read + normalize + write_las(paste0(tempdir(), "/*_norm.las"))
#' exec(pipeline, on = f)
#' @export
write_las = function(ofile = paste0(tempdir(), "/*.las"), filter = "", keep_buffer = FALSE)
{
  ans <- list(algoname = "write_las", filter = filter, output = ofile, keep_buffer = keep_buffer)
  set_lasr_class(ans)
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
  ans <- list(algoname = "write_vpc", output = ofile, absolute = absolute_path, use_gpstime = use_gpstime)
  set_lasr_class(ans)
}

#' Write spatial indexing .lax files
#'
#' Creates a .lax file for each `.las` or `.laz` file. A .lax file contains spatial indexing information.
#' Spatial indexing drastically speeds up tile buffering and spatial queries. In lasR, it is mandatory
#' to have spatially indexed point clouds, either using .lax files or .copc.laz files. If the processed file
#' collection is not spatially indexed, a `write_lax()` file will automatically be added at the beginning
#' of the pipeline (see Details).
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
  ans <- list(algoname = "write_lax", embedded = embedded, overwrite = overwrite)
  set_lasr_class(ans)
}

# ==== INTERNALS =====

generate_uid <- function(size = 8)
{
  paste(sample(c(letters[1:6], as.character(0:8)), size, replace = TRUE), collapse = "")
}

set_lasr_class = function(x, raster = FALSE, vector = FALSE)
{
  x[["uid"]] = generate_uid()

  if (is.null(x[["output"]])) x[["output"]] = ""
  if (is.null(x[["filter"]])) x[["filter"]] = ""

  x <- Filter(Negate(is.null), x)

  x[["output"]] = normalizePath(x[["output"]], mustWork = FALSE)
  x[["filter"]] = as.character(x[["filter"]])

  cl <- c("LASRalgorithm", "list")
  if (raster) cl <- c(cl, "LASRraster")
  if (vector) cl <- c(cl, "LASRvector")

  class(x) <- cl
  x <- list(x)
  names(x) <- x[[1]][["algoname"]]
  class(x) <- c("LASRpipeline", "list")
  return(x)
}

get_stage = function(x)
{
  call = deparse(substitute(x))

  if (methods::is(x, "LASRalgorithm"))
    return(x)

  if (methods::is(x, "LASRpipeline"))
  {
    if (length(x) != 1L)
      stop(paste("Cannot input a complex pipeline.", call, "has", length(x), "stages"))

    return(x[[1]])
  }

  stop(paste("The stage", call, "must be a 'LASRalgorithm'"))
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


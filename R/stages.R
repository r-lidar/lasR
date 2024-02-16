# ===== A =====

#' Add attributes to a LAS file
#'
#' According to the \href{https://www.asprs.org/a/society/committees/standards/LAS_1_4_r13.pdf}{LAS specifications},
#' a LAS file contains a core of defined attributes, such as XYZ coordinates, intensity, return number,
#' and so on, for each point. It is possible to add supplementary attributes. This algorithm adds an
#' extra bytes attribute to the points. Values are zeroed. It edits the point cloud but returns nothing.
#'
#' @param name character. The name of the extra bytes attribute to add to the file.
#' @param description character. A short description of the extra bytes attribute to add to the file (32 characters).
#' @param data_type character. The data type of the extra bytes attribute. Can be "uchar", "char", "ushort",
#' "short", "uint", "int", "uint64", "int64", "float", "double".
#' @param scale,offset numeric. The scale and offset of the data. See LAS specification.
#'
#' @export
#'
#' @examples
#' f <- system.file("extdata", "Example.las", package = "lasR")
#' fun <- function(data) { data$RAND <- runif(nrow(data), 0, 100); return(data) }
#' pipeline <- reader(f) +
#'   add_extrabytes("float", "RAND", "Random numbers") +
#'   callback(fun, expose = "xyz")
#' processor(pipeline)
add_extrabytes = function(data_type, name, description, scale = 1, offset = 0)
{
  types <- c("uchar", "char", "ushort", "short", "uint", "int", "uint64", "int64", "float", "double")
  data_type <- match.arg(data_type, types)
  #data_type <- which(data_type == types)

  ans <- list(algoname = "add_extrabytes", data_type = data_type, name = name, description = description, scale = scale, offset = offset)
  set_lasr_class(ans)
}

#' Aggregate a point cloud and compute metrics for each group
#'
#' Aggregate a point cloud and compute metrics for each group. The currently supported grouping method
#' is per pixel with a raster output. This is the algorithm returned when using \link{rasterize} with
#' a user-defined expression. At this stage, this algorithm only aims to drive \link{rasterize}.
#'
#' @param res numeric. The resolution of the raster.
#' @param call expression. User-defined expression.
#' @template param-filter
#' @template param-ofile
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#' read <- reader(f)
#' pipeline <- read + summarise()
#' ans <- processor(pipeline)
#' # plot(ans[[1]])
#' # plot(ans[[2]])
#'
#' @seealso
#' \link{rasterize}
#' @noRd
aggregate = function(res, call, filter = "", ofile = tempfile(fileext = ".tif"), ...)
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
  set_lasr_class(ans)
}

# ===== C =====


#' Call a user-defined function on the point cloud
#'
#' Call a user-defined function on the point cloud. The function receives a `data.frame` with the
#' point cloud. Its first input must be the point cloud. If the function returns anything other than
#' a `data.frame` with the same number of points, the output is stored and returned at the end. However,
#' if the output is a `data.frame` with the same number of points, it updates the point cloud. This
#' function can, therefore, be used to modify the point cloud using a user-defined function.
#'
#' In `lasR`, the point cloud is not exposed to R in a `data.frame` like in lidR. It is stored internally
#' in a C++ structure and cannot be seen or modified directly by users using R code. The `callback` function
#' is the only algorithm that allows direct interaction with the point cloud by **copying** it
#' temporarily into a `data.frame` to apply a user-defined function.\cr\cr
#' **expose:** the 'expose' argument specifies the data that will actually be exposed to R. For example,
#' 'xyzia' means that the x, y, and z coordinates, the intensity, and the scan angle will be loaded.
#' The supported entries are t - gpstime, a - scan angle, i - intensity, n - number of returns,
#' r - return number, c - classification, s - synthetic flag, k - keypoint flag, w - withheld flag,
#' o - overlap flag (format 6+), u - user data, p - point source ID, e - edge of flight line flag,
#' d - direction of scan flag, R - red channel of RGB color, G - green channel of RGB color,
#' B - blue channel of RGB color, N - near-infrared channel, C - scanner channel (format 6+)
#' Also numbers from 1 to 9 for the extra bytes data numbers 1 to 9. 0 enables all extra bytes to be
#' loaded, and '*' is the wildcard that enables everything to be loaded from the LAS file.
#'
#' @param fun numeric. The resolution of the raster.
#' @param expose character. Expose only attributes of interest to save memory (see details).
#' @param drop_buffer bool. If false, does not expose the point from the buffer.
#' @param no_las_update bool. If the user-defined function returns a data.frame, this is supposed to
#' update the point cloud. Can be disabled.
#' @param ... parameters of function `fun`
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#'
#' # There is no function in lasR to read the data in R. Let's create one
#' read_las <- function(f)
#' {
#'   load <- function(data) { return(data) }
#'   read <- reader(f)
#'   call <- callback(load, expose = "xyzi", no_las_update = TRUE)
#'   return (processor(read + call))
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
#' read <- reader(f)
#' call <- callback(convert_intensity_in_range, expose = "xyzi", min = 0, max = 255)
#' write <- write_las()
#' pipeline <- read + call + write
#' ans <- processor(pipeline)
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

#' Classify isolated points
#'
#' The algorithm identifies points that have only a few other points in their surrounding
#' 3 x 3 x 3 = 27 voxels and edits the points to assign a target classification. Used with class 18,
#' it classifies points as noise and is similar to \href{https://rapidlasso.de/lasnoise/}{lasnoise}
#' from lastools. This algorithm modifies the point cloud in the pipeline but does not produce any output.
#'
#' @param res numeric. Resolution of the voxels.
#' @param n integer. The maximal number of 'other points' in the 27 voxels.
#' @param class integer. The class to assign to the points that match the condition.
#'
#' @export
classify_isolated_points = function(res = 5, n = 6L, class = 18L)
{
  ans <- list(algoname = "classify_isolated_points", res = res, n = n, class = class)
  set_lasr_class(ans)
}

# ===== H =====

#' Contour of a Delaunay triangulation
#'
#' This algorithm uses a Delaunay triangulation and computes its contour. The contour of a strict
#' Delaunay triangulation is the convex hull, but in lasR, the triangulation has a `max_edge` argument.
#' Thus, the contour might be a convex hull with holes.
#'
#' @param mesh NULL or LASRalgorithm. A `triangulate` algorithm. If NULL take the bounding box of the
#' header of each file.
#' @template param-ofile
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#' read <- reader(f)
#' tri <- triangulate(20, filter = "-keep_class 2")
#' contour <- hulls(tri)
#' pipeline <- read + tri + contour
#' ans <- processor(pipeline)
#' plot(ans)
#'
#' @seealso
#' \link{triangulate}
#'
#' @export
#' @md
hulls = function(mesh = NULL, ofile = tempfile(fileext = ".gpkg"))
{
  if (!is.null(mesh))
  {
    if (!methods::is(mesh, "LASRpipeline")) stop("triangulator must be a 'LASRpipeline'") # nocov
    if (length(mesh) != 1L) stop("cannot input a complex pipeline")  # nocov
    if (mesh[[1]]$algoname != "triangulate") stop("the algorithm must be 'triangulate'")  # nocov
    ans <- list(algoname = "hulls", connect = mesh[[1]][["uid"]], output = ofile)
  }
  else
  {
    ans <- list(algoname = "hulls", output = ofile)
  }

  set_lasr_class(ans)
}

# ===== L =====

#' Local Maximum
#'
#' The Local Maximum algorithm identifies points that are locally maximum. The window size is
#' fixed and circular. This algorithm does not modify the point cloud. It produces a derived product
#' in vector format.
#'
#' @param ws numeric. Diameter of the moving window used to detect the local maxima in the units of
#' the input data (usually meters).
#' @param min_height numeric. Minimum height of a local maximum. Threshold below which a point cannot be a
#' local maximum. Default is 2.
#' @param use_attribute character. By default the local maximum is performed on the coordinate Z. Can also be
#' the name of an extra bytes attribute such as 'HAG' if it exists. Can also be 'Intensity' but there is
#' probably no use case for that one.
#'
#' @template param-filter
#' @template param-ofile
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package = "lasR")
#' read <- reader(f)
#' lmf <- local_maximum(3)
#' ans <- processor(read + lmf)
#' ans
#' @export
local_maximum = function(ws, min_height = 2, filter = "", ofile = tempfile(fileext = ".gpkg"), use_attribute = "Z")
{
  ans <- list(algoname = "local_maximum", ws = ws, min_height = min_height, filter = filter, output = ofile, use_attribute = use_attribute)
  set_lasr_class(ans)
}

# ===== N ====

nothing = function(read = FALSE, stream = FALSE)
{
  ans <- list(algoname = "nothing", read = read, stream = stream)
  set_lasr_class(ans)
}

# ===== P ====

#' Pits and spikes filling
#'
#' Pits and spikes filling for raster. Typically used for post-processing CHM. This algorithm
#' is from St-Onge 2008 (see reference).
#'
#' @param raster LASRalgorithm. An algorithm that produces a raster.
#' @param lap_size integer. Size of the Laplacian filter kernel (integer value, in pixels).
#' @param thr_lap numeric. Threshold Laplacian value for detecting a cavity (all values above this
#' value will be considered a cavity). A positive value.
#' @param thr_spk numeric. Threshold Laplacian value for detecting a spike (all values below this
#' value will be considered a spike). A negative value.
#' @param med_size integer. Size of the median filter kernel (integer value, in pixels).
#' @param dil_radius integer. Dilation radius (integer value, in pixels).
#' @template param-ofile
#'
#' @references
#' St-Onge, B., 2008. Methods for improving the quality of a true orthomosaic of Vexcel UltraCam
#' images created using alidar digital surface model, Proceedings of the Silvilaser 2008, Edinburgh,
#' 555-562. https://citeseerx.ist.psu.edu/document?repid=rep1&type=pdf&doi=81365288221f3ac34b51a82e2cfed8d58defb10e
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package="lasR")
#'
#' reader <- reader(f, filter = "-keep_first")
#' tri <- triangulate()
#' chm <- rasterize(0.25, tri)
#' pit <- pit_fill(chm)
#' u <- processor(reader + tri + chm + pit)
#'
#' chm <- u[[1]]
#' sto <- u[[2]]
#'
#' #terra::plot(c(chm, sto), col = lidR::height.colors(25))
#' @export
pit_fill = function(raster, lap_size = 3L, thr_lap = 0.1, thr_spk = -0.1, med_size = 3L, dil_radius = 0L, ofile = tempfile(fileext = ".tif"))
{
  if (!methods::is(raster, "LASRpipeline")) stop("rasterizator must be a 'LASRpipeline'")  # nocov
  if (length(raster) != 1L) stop("cannot input a complex pipeline")  # nocov
  if (raster[[1]]$algoname != "rasterize") stop("the algorithm must be 'rasterize'")  # nocov

  ans <- list(algoname = "pit_fill", connect = raster[[1]][["uid"]],  lap_size = lap_size, thr_lap = thr_lap, thr_spk = thr_spk, med_size = med_size, dil_radius = dil_radius, output = ofile)
  set_lasr_class(ans)
}

# ===== R =====

#' Rasterize a point cloud
#'
#' Rasterize a point cloud using different approaches. This algorithm does not modify the point cloud.
#' It produces a derived product in raster format.
#'
#' If `operators` is a user-defined expression, the function must return either a vector of numbers
#' or a list with atomic numbers. To assign a band name to the raster the vector or the list must be named.
#' These are valid operators:
#' ```
#' f = function(x) { return(mean(x)) }
#' g = function(x,y) { return(c(avg = mean(x), med = median(y))) }
#' h = function(x) { return(list(a = mean(x), b = median(x))) }
#' rasterize(10, f(Intensity))
#' rasterize(10, g(Z, Intensity))
#' rasterize(10, h(Z))
#' ````
#' \cr
#' If the argument `res` is a vector with two numbers, the first number represents the resolution of
#' the output raster, and the second number represents the size of the windows used to compute the metrics.
#' This approach is called Buffered Area Based Approach (BABA). In classical rasterization, the metrics
#' are computed independently for each pixel using the points. For example, predicting a resource typically
#' involves computing metrics with a 400 m2 pixel, resulting in a raster with a resolution of 20 m.
#' It is not possible to achieve a finer granularity with this method. However, with buffered rasterization,
#' it is possible to compute the raster at a resolution of 10 m (i.e., computing metrics every 10 meters)
#' while using 20 x 20 windows for metric computation. In this case, the windows overlap, essentially
#' creating a moving window effect. This option does not makes when rasterizing a triangulation and
#' the second value is not considered in this case
#'
#'
#' @param res numeric. The resolution of the raster. Can be a vector with two resolutions.
#' In this case it does not correspond to the x and y resolution but to a buffed rasterization.
#' (see details)
#' @param operators Can be a character vector. "min", "max" and "count" are accepted. Can also
#' rasterize a triangulation if the input is a LASRalgorithm for triangulation (see examples).
#' Can also be a user-defined expression (see example and details).
#' @template param-filter
#' @template param-ofile
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader(f)
#' tri  <- triangulate(filter = "-keep_class 2")
#' dtm  <- rasterize(1, tri) # input is a triangulation algorithm
#' avgi <- rasterize(10, mean(Intensity)) # input is a user expression
#' chm  <- rasterize(2, "max") # input is a character vector
#' pipeline <- read + tri + dtm + avgi + chm
#' ans <- processor(pipeline)
#' ans[[1]]
#' ans[[2]]
#' ans[[3]]
#'
#' # Demonstration of buffered rasterization
#'
#' # A good resolution for computing point density is 4 meters.
#' c0 <- rasterize(4, "count")
#'
#' # Computing point density at too fine a resolution doesn't make sense since there is
#' # either zero or one point per pixel. Therefore, producing a point density raster with
#' # a 1 m resolution is not feasible with classical rasterization.
#' c1 <- rasterize(1, "count")
#'
#' # Using a buffered approach, we can produce a raster with a 1-meter resolution where
#' # the metrics for each pixel are computed using a 4-meter window.
#' c2  <- rasterize(c(1,4), "count")
#'
#' pipeline = read + c0 + c1 + c2
#' res <- processor(pipeline)
#' terra::plot(res[[1]]/16)  # divide by 16 to get the density
#' terra::plot(res[[2]]/1)   # divide by 1 to get the density
#' terra::plot(res[[3]]/16)  # divide by 16 to get the density
#' @export
#' @md
rasterize = function(res, operators = "max", filter = "", ofile = tempfile(fileext = ".tif"))
{
  class <- tryCatch({class(operators)}, error = function(x) return("call"))

  res_raster  <- res[1]
  res_window <- res[1]
  if (length(res) > 1L)
    res_window <- res[2]

  if (class == "call")
  {
    env <- new.env(parent = parent.frame())
    return(aggregate_q(res, substitute(operators), filter, ofile, env))
  }

  if (methods::is(operators, "LASRpipeline"))
  {
    if (length(operators) == 1)
      ans <- list(algoname = "rasterize", res = res_raster, connect = operators[[1]][["uid"]], filter = filter, output = ofile)
    else
      stop("cannot input a complex pipeline")
  }
  else if (is.character(operators))
  {
    supported_operators <- c("max", "min", "count")
    valid <- operators %in% supported_operators
    if (!all(valid)) stop("Non supported operators")
    id <- match(operators, supported_operators)
    ans <- list(algoname = "rasterize", res = res_raster, window = res_window, method = id, filter = filter, output = ofile)
  }
  else
  {
    stop("Invalid operators") # nocov
  }
  set_lasr_class(ans)
}

#' Initialize the pipeline
#'
#' This is the first stage that must be called in each pipeline. It specifies which files must be read.
#' The stage does nothing and returns nothing if it is not associated to another processing stage.
#' It only initializes the pipeline. `reader()` is the main function that dispatches into to other
#' functions. `reader_*()` reads from LAS/LAZ files on disk.`reader_coverage()` processes the entire
#' point cloud. `reader_circles()` and `reader_rectangles()` read and process only some selected regions
#' of interest.
#'
#' @param x Can be the paths of the files to use, the path of the folder in which the files are stored,
#' the path to a [virtual point cloud](https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/)
#' file or a `data.frame` containing hte point cloud. It supports also a `LAScatalog` or a `LAS` objects
#' from `lidR`.
#' @template param-filter
#' @param buffer numeric. Each file is read with a buffer. The default is 0, which does not mean that
#' the file won't be buffered. It means that the internal routine knows if a buffer is needed and will
#' pick the greatest value between the internal suggestion and this provided value.
#' @param xc,yc,r numeric. Circle centres and radius or radii.
#' @param xmin,ymin,xmax,ymax numeric. Coordinates of the rectangles
#' @param ... passed to other readers
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#' read <- reader(f)
#' ans <- processor(read)
#'
#' @export
#' @md
reader = function(x, filter = "", buffer = 0, ...)
{
  p <- list(...)
  circle <- !is.null(p$xc)
  rectangle <-!is.null(p$xmin)

  if (circle) return(reader_circles(x, p$xc, p$yc, p$r, filter = filter, buffer = buffer, ...))
  if (rectangle) return(reader_rectangles(x, p$xmin, p$ymin, p$xmax, p$ymax, filter = filter, buffer = buffer, ...))
  return(reader_coverage(x, filter = filter, buffer = buffer, ...))
}

#' @export
#' @rdname reader
reader_coverage = function(x, filter = "", buffer = 0, ...)
{
  if (methods::is(x, "LAScatalog") | is.character(x)) return(reader_files_coverage(x, filter, buffer, ...))
  if (methods::is(x, "LAS") | is.data.frame(x)) return(reader_dataframe_coverage(x, filter, buffer, ...))
  stop("'x' must be a character vector, a data.frame or a LAS* object from lidR.") # nocov
}

#' @export
#' @rdname reader
reader_circles = function(x, xc, yc, r, filter = "", buffer = 0, ...)
{
  if (methods::is(x, "LAScatalog") | is.character(x)) return(reader_files_circles(x, xc, yc, r, filter, buffer, ...))
  if (methods::is(x, "LAS") | is.data.frame(x)) return(reader_dataframe_circles(x, xc, yc, r, filter, buffer, ...))
  stop("'x' must be a character vector, a data.frame or a LAS* object from lidR.") # nocov
}

#' @export
#' @rdname reader
reader_rectangles = function(x, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
{
  if (methods::is(x, "LAScatalog") | is.character(x)) return(reader_files_rectangles(x, xmin, ymin, xmax, ymax, filter, buffer, ...))
  if (methods::is(x, "LAS") | is.data.frame(x)) return(reader_dataframe_rectangles(x, xmin, ymin, xmax, ymax, filter, buffer, ...))
  stop("'x' must be a character vector, a data.frame or a LAS* object from lidR.") # nocov
}

reader_files_coverage = function(files, filter = "", buffer = 0, ...)
{
  p <- list(...)
  noprocess = p$noprocess

  if (methods::is(files, "LAScatalog"))
  {
    files <- files$filename
    if (!is.null(files$processed)) noprocess = !files$processed
  }

  if (!is.character(files))
  {
    stop("'files' must be character or a LAScatalog")  # nocov
  }

  if (!is.null(noprocess))
  {
    if (length(noprocess) != length(files))
      stop("'noprocess' and 'files' have different length")
  }

  files <- normalizePath(files)
  ans <- list(algoname = "reader_las", files = files, filter = filter, buffer = buffer, noprocess = noprocess)
  set_lasr_class(ans)
}

reader_files_circles = function(files, xc, yc, r, filter = "", buffer = 0, ...)
{
  stopifnot(length(xc) == length(yc))
  if (length(r) == 1) r <- rep(r, length(xc))
  if (length(r) > 1) stopifnot(length(xc) == length(r))

  ans <- reader_files_coverage(files, filter, buffer, ...)
  ans[[1]]$xcenter <- xc
  ans[[1]]$ycenter <- yc
  ans[[1]]$radius <- r
  ans
}

reader_files_rectangles = function(files, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
{
  stopifnot(length(xmin) == length(ymin), length(xmin) == length(xmax), length(xmin) == length(ymax))

  ans <- reader_files_coverage(files, filter, buffer)
  ans[[1]]$xmin <- xmin
  ans[[1]]$ymin <- ymin
  ans[[1]]$xmax <- xmax
  ans[[1]]$ymax <- ymax
  ans
}

reader_dataframe_coverage = function(dataframe, filter = "", buffer = 0, ...)
{
  accuracy = c(0,0,0)
  if (methods::is(dataframe, "LAS"))
  {
    accuracy <- c(dataframe@header@PHB[["X scale factor"]], dataframe@header@PHB[["Y scale factor"]], dataframe@header@PHB[["Z scale factor"]])
    crs <- dataframe@crs$wkt
    dataframe <- dataframe@data
    attr(dataframe, "accuracy") <- accuracy
    attr(dataframe, "crs") <- crs
  }

  stopifnot(is.data.frame(dataframe))

  crs <- attr(dataframe, "crs")
  if (is.null(crs)) crs = ""
  if (!is.character(crs) & length(crs != 1)) stop("The CRS of this data.frame is not a WKT string")

  acc <- attr(dataframe, "accuracy")
  if (is.null(acc)) acc = c(0, 0, 0)
  if (!is.numeric(acc) & length(acc) != 3L) stop("The accuracy of this data.frame is not valid")

  ans <- list(algoname = "reader_dataframe", dataframe = dataframe, accuracy = acc, crs = crs, filter = filter, buffer = buffer)
  set_lasr_class(ans)
}

reader_dataframe_circles = function(dataframe, xc, yc, r, filter = "", buffer = 0, ...)
{
  stopifnot(length(xc) == length(yc))
  if (length(r) == 1) r <- rep(r, length(xc))
  if (length(r) > 1) stopifnot(length(xc) == length(r))

  ans <- reader_dataframe_coverage(dataframe, filter, buffer)
  ans[[1]]$xcenter <- xc
  ans[[1]]$ycenter <- yc
  ans[[1]]$radius <- r
  ans
}

reader_dataframe_rectangles = function(dataframe, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
{
  stopifnot(length(xmin) == length(ymin), length(xmin) == length(xmax), length(xmin) == length(ymax))

  ans <- reader_dataframe_coverage(dataframe, filter, buffer)
  ans[[1]]$xmin <- xmin
  ans[[1]]$ymin <- ymin
  ans[[1]]$xmax <- xmax
  ans[[1]]$ymax <- ymax
  ans
}

#' Region growing
#'
#' Region growing for individual tree segmentation based on Dalponte and Coomes (2016) algorithm (see reference).
#' Note that this algorithm strictly performs segmentation, while the original method described in
#' the manuscript also performs pre- and post-processing tasks. Here, these tasks are expected to be
#' done by the user in separate functions.
#'
#' @param raster LASRalgoritm an algorithm producing a raster.
#' @param seeds LASRalgoritm an algorithm producing points used as seeds.
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
#' @references
#' Dalponte, M. and Coomes, D. A. (2016), Tree-centric mapping of forest carbon density from
#' airborne laser scanning and hyperspectral data. Methods Ecol Evol, 7: 1236–1245. doi:10.1111/2041-210X.12575.
#'
#' @export
#'
#' @examples
#' f <- system.file("extdata", "MixedConifer.las", package="lasR")
#'
#' reader <- reader(f, filter = "-keep_first")
#' reader <- reader(f, filter = "-keep_first")
#' chm <- rasterize(1, "max")
#' lmx <- local_maximum(5)
#' tree <- region_growing(chm, lmx, max_cr = 10)
#' u <- processor(reader + chm + lmx + tree)
#'
#' @md
region_growing = function(raster, seeds, th_tree = 2, th_seed = 0.45, th_cr = 0.55, max_cr = 20, ofile = tempfile(fileext = ".tif"))
{
  if (!methods::is(raster, "LASRpipeline")) stop("'chm' must be a 'LASRpipeline'")  # nocov
  if (!methods::is(seeds, "LASRpipeline")) stop("'chm' must be a 'LASRpipeline'")  # nocov
  if (length(raster) > 1 || length(seeds) > 1) stop("cannot input a complex pipeline")  # nocov

  ans <- list(algoname = "region_growing", connect1 = seeds[[1]][["uid"]], connect2 = raster[[1]][["uid"]], th_tree = th_tree, th_seed = th_seed, th_cr = th_cr, max_cr = max_cr, output = ofile)
  set_lasr_class(ans)
}

# ==== S =====

#' Sample the point cloud keeping one random point per units
#'
#' Sample the point cloud, keeping one random point per pixel or per voxel. This algorithm modifies
#' the point cloud in the pipeline but does not produce any output.
#'
#' @param res numeric. voxel resolution
#' @template param-filter
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader(f)
#' vox <- sampling_voxel(5)
#' write <- write_las()
#' pipeline <- read + vox + write
#' processor(pipeline)
#' @export
#' @rdname sampling
sampling_voxel = function(res = 2, filter = "")
{
  ans <- list(algoname = "sampling_voxel", res = res, filter = filter)
  set_lasr_class(ans)
}

#' @export
#' @rdname sampling
sampling_pixel = function(res = 2, filter = "")
{
  ans <- list(algoname = "sampling_pixel", res = res, filter = filter)
  set_lasr_class(ans)
}

#' Summary
#'
#' Summarize the dataset by counting the number of points, first returns, classes. It also produces
#' a histogram of Z and Intensity. This algorithm does not modify the point cloud. It produces a
#' summary as a `list`.
#'
#' @param zwbin,iwbin numeric. Width of the bins for the histograms of Z and Intensity.
#' @template param-filter
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader(f)
#' pipeline <- read + summarise()
#' ans <- processor(pipeline)
#' ans
#' @export
summarise = function(zwbin = 2, iwbin = 25, filter = "")
{
  ans <- list(algoname = "summarise", filter = filter, zwbin = zwbin, iwbin = iwbin)
  set_lasr_class(ans)
}

# ===== T =====

#' Delaunay triangulation
#'
#' Delaunay triangulation. Can be used to build a DTM, a CHM, normalize a point cloud, or any other
#' application. This algorithm is typically used as an intermediate process without an output file.
#' This algorithm does not modify the point cloud.
#'
#' @param max_edge numeric. Maximum edge length of a triangle in the Delaunay triangulation. If a
#' triangle has an edge length greater than this value, it will be removed. If max_edge = 0, no trimming
#' is done (see examples).
#' @param use_attribute character. By default the triangulation is performed on the coordinate Z. Can also be
#' the name of an extra bytes attribute such as 'HAG' if it exists. Can also be 'Intensity'.
#'
#' @template param-filter
#' @template param-ofile
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader(f)
#' tri1 <- triangulate(25, filter = "-keep_class 2", ofile = tempfile(fileext = ".gpkg"))
#' filter <- "-keep_last -keep_random_fraction 0.1"
#' tri2 <- triangulate(filter = filter, ofile = tempfile(fileext = ".gpkg"))
#' pipeline <- read + tri1 + tri2
#' ans <- processor(pipeline)
#' #plot(ans[[1]])
#' #plot(ans[[2]])
#' @export
triangulate = function(max_edge = 0, filter = "", ofile = "", use_attribute = "Z")
{
  ans <- list(algoname = "triangulate", max_edge = max_edge, filter = filter, output = ofile, use_attribute = use_attribute)
  set_lasr_class(ans)
}

#' Transform a point cloud using a triangulation
#'
#' This algorithm uses a Delaunay triangulation and, for each point, it linearly interpolates the
#' triangulation to retrieve the value of the mesh at the exact location of the point. Then it
#' performs an operation with this value to modify the point cloud. This can typically be used
#' to build a normalization algorithm. This algorithm modifies the point cloud in the pipeline but
#' does not produce any output.
#'
#' @param stage LASRpipeline. A stage that produces a triangulation or a raster.
#' @param operator string. '-' and '+' are supported.
#' @param store_in_attribute numeric. Use an extra bytes attribute to store the result.
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#'
#' # There is a normalize pipeline in lasR but let's create one almost equivalent
#' mesh  <- triangulate(filter = keep_ground())
#' trans <- transform_with(mesh)
#' pipeline <- reader(f) + mesh + trans + write_las()
#' ans <- processor(pipeline)
#'
#' @seealso
#' \link{reader}
#' \link{triangulate}
#' \link{write_las}
#' @export
transform_with = function(stage, operator = "-", store_in_attribute = "")
{
  if (!methods::is(stage, "LASRpipeline")) stop("the stage must be a 'LASRpipeline' object")  # nocov
  if (length(stage) != 1L) stop("cannot input a complex pipeline")  # nocov
  if (!stage[[1]]$algoname %in% c("triangulate", "rasterize", "aggregate")) stop("the stage must be a triangulation or a raster stage")  # nocov

  ans <- list(algoname = "transform_with", connect = stage[[1]][["uid"]], operator = operator, store_in_attribute = store_in_attribute)
  set_lasr_class(ans)
}

# ===== W ====

#' Write LAS or LAZ files
#'
#' Write a LAS or LAZ file at any step of the pipeline (typically at the end). Unlike other algorithms,
#' the output won't be written into a single large file but in multiple tiled files corresponding
#' to the original collection of files.
#'
#' @param ofile character. Output file names. The string must contain a wildcard * so the wildcard can
#' be replaced by the algorithm name of the original tile and preserve the tiling pattern. If the wildcard
#' is omitted, everything will be written into a single file. This may be the desired behaviour in some
#' circumstances, e.g., to merge some files.
#' @param keep_buffer bool. The buffer is removed to write file but it can be preserved.
#' @template param-filter
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' read <- reader(f)
#' tri  <- triangulate(filter = "-keep_class 2")
#' normalize <- tri + transform_with(tri)
#' pipeline <- read + normalize + write_las(paste0(tempdir(), "/*_norm.las"))
#' processor(pipeline)
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
#' @param ofile character. The file path with extnsion .vpc where to write the virtual point cloud file
#' @references
#' \url{https://www.lutraconsulting.co.uk/blog/2023/06/08/virtual-point-clouds/}\cr
#' \url{https://github.com/PDAL/wrench/blob/main/vpc-spec.md}
#' @examples
#' \dontrun{
#' pipeline = reader("folder/") + write_vpc("folder/dataset.vpc")
#' }
#' @export
write_vpc = function(ofile)
{
  ans <- list(algoname = "write_vpc", output = ofile)
  set_lasr_class(ans)
}

# ==== INTERNALS =====

generate_uid <- function(size = 6)
{
  paste(sample(c(letters, LETTERS, as.character(0:9)), size, replace = TRUE), collapse = "")
}

set_lasr_class = function(x)
{
  x[["uid"]] = generate_uid()

  if (is.null(x[["output"]])) x[["output"]] = ""
  if (is.null(x[["filter"]])) x[["filter"]] = ""

  x <- Filter(Negate(is.null), x)

  x[["output"]] = normalizePath(x[["output"]], mustWork = FALSE)

  class(x) <- "LASRalgorithm"
  x = list(x)
  class(x) <- "LASRpipeline"
  return(x)
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


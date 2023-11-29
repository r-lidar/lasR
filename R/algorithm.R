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
aggregate = function(res, call, filter = "", ofile = tempfile(fileext = ".tif"))
{
  call <- substitute(call)
  env <- new.env(parent=parent.frame())
  aggregate_q(res, call, filter, ofile, env)
}

aggregate_q = function(res, call, filter, ofile, env)
{
  call <- as.call(call)
  ans <- list(algoname = "aggregate", res = res, call = call, filter = filter, output = ofile, env = env)
  set_lasr_class(ans)
}

# ===== B =====

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
#' contour <- boundaries(tri)
#' pipeline <- read + tri + contour
#' ans <- processor(pipeline)
#' plot(ans)
#'
#' @seealso
#' \link{triangulate}
#'
#' @export
#' @md
boundaries = function(mesh = NULL, ofile = tempfile(fileext = ".gpkg"))
{
  if (!is.null(mesh))
  {
    if (!methods::is(mesh, "LASRpipeline")) stop("triangulator must be a 'LASRpipeline'")
    if (length(mesh) != 1L) stop("cannot input a complex pipeline")
    if (mesh[[1]]$algoname != "triangulate") stop("the algorithm must be 'triangulate'")
    ans <- list(algoname = "boundaries", connect = mesh[[1]][["uid"]], output = ofile)
  }
  else
  {
    ans <- list(algoname = "boundaries", output = ofile)
  }

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
classify_isolated_points = function(res = 5, n = 6, class = 18L)
{
  ans <- list(algoname = "classify_isolated_points", res = res, n = n, class = class)
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



nothing = function()
{
  ans <- list(algoname = "nothing")
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
  if (!methods::is(raster, "LASRpipeline")) stop("rasterizator must be a 'LASRpipeline'")
  if (length(raster) != 1L) stop("cannot input a complex pipeline")
  if (raster[[1]]$algoname != "rasterize") stop("the algorithm must be 'rasterize'")

  ans <- list(algoname = "pit_fill", connect = raster[[1]][["uid"]],  lap_size = lap_size, thr_lap = thr_lap, thr_spk = thr_spk, med_size = med_size, dil_radius = dil_radius, output = ofile)
  set_lasr_class(ans)
}

# ===== R =====

#' Rasterize a point cloud
#'
#' Rasterize a point cloud using different approaches. This algorithm does not modify the point cloud.
#' It produces a derived product in raster format.
#'
#' @param res numeric. The resolution of the raster.
#' @param operators Can be a character vector. "min", "max" and "count" are accepted. Can also
#' rasterize a triangulation if the input is a LASRalgorithm for triangulation (see examples).
#' Can also be a user-defined expression (see example).
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
#' @export
rasterize = function(res, operators = "max", filter = "", ofile = tempfile(fileext = ".tif"))
{
  class <- tryCatch({class(operators)}, error = function(x) return("call"))

  if (class == "call")
  {
    env <- new.env(parent = parent.frame())
    return(aggregate_q(res, substitute(operators), filter, ofile, env))
  }

  if (methods::is(operators, "LASRpipeline"))
  {
    if (length(operators) == 1)
      ans <- list(algoname = "rasterize", res = res, connect = operators[[1]][["uid"]], filter = filter, output = ofile)
    else
      stop("cannot input a complex pipeline")
  }
  else if (is.character(operators))
  {
    supported_operators <- c("max", "min", "count")
    valid <- operators %in% supported_operators
    if (!all(valid)) stop("Non supported operators")
    id <- match(operators, supported_operators)
    ans <- list(algoname = "rasterize", "res" = res, "method" = id, filter = filter, output = ofile)
  }
  else
  {
    stop("Invalid operators")
  }
  set_lasr_class(ans)
}

#' Initialize the pipeline
#'
#' This is the first algorithm that must be called in each pipeline. It specifies which files must be read.
#' The algorithm does nothing and returns nothing. It only initializes the pipeline. `reader()` is equivalent
#' to `read_coverage()`. `read_coverage()` reads all the files one by one, `read_circles()` and `read_rectangles()`
#' read only some selected regions of interest one by one.
#'
#' @param files character. The path of the files to use. Supports a `LAScatalog` from `lidR`. Supports a path
#' to a folder.
#' @template param-filter
#' @param buffer numeric. Each file is read with a buffer. The default is 0, which does not mean that
#' the file won't be buffered. It means that the internal routine knows if a buffer is needed and will
#' pick the greatest value between the internal suggestion and this value if needed.
#' @param x,y,r numeric. Circle centres and radius or radii.
#' @param xmin,ymin,xmax,ymax numeric. Coordinates of the rectangles
#' @param ... Unused
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package = "lasR")
#' read <- reader(f)
#' ans <- processor(read)
#'
#' @export
#' @md
reader = function(files, filter = "", buffer = 0, ...)
{
  if (methods::is(files, "LAScatalog")) files <- files$filename
  if (!is.character(files)) stop("Files must be character")

  finfo <- file.info(files)

  if (all(is.na(finfo$isdir)))
    stop(paste0(files, " does not exist."))
  else if (all(!finfo$isdir))
    files <- normalizePath(files)
  else
  {
    p <- list(...)
    p$path <- files
    p$full.names <- TRUE
    p$pattern <- "(?i)\\.la(s|z)$"
    files <- do.call(list.files, p)
  }

  nexist <- sum(!file.exists(files))
  if (nexist > 0) stop(paste0(nexist, " file(s) not existing"))

  ans <- list(algoname = "reader", files = files, filter = filter, buffer = buffer)
  set_lasr_class(ans)
}

#' @export
#' @rdname reader
reader_coverage = reader

#' @export
#' @rdname reader
reader_circles = function(files, x, y, r, filter = "", buffer = 0, ...)
{
  stopifnot(length(x) == length(y))
  if (length(r) == 1) r <- rep(r, length(x))
  if (length(r) > 1) stopifnot(length(x) == length(r))

  ans <- reader(files, filter, buffer, ...)
  ans[[1]]$xcenter <- x
  ans[[1]]$ycenter <- y
  ans[[1]]$radius <- r
  ans
}

#' @export
#' @rdname reader
reader_rectangles = function(files, xmin, ymin, xmax, ymax, filter = "", buffer = 0, ...)
{
  stopifnot(length(xmin) == length(ymin), length(xmin) == length(xmax), length(xmin) == length(ymax))

  ans <- reader(files, filter, buffer, ...)
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
  if (!methods::is(raster, "LASRpipeline")) stop("'chm' must be a 'LASRpipeline'")
  if (!methods::is(seeds, "LASRpipeline")) stop("'chm' must be a 'LASRpipeline'")
  if (length(raster) > 1 || length(seeds) > 1) stop("cannot input a complex pipeline")

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
sampling_pixel = function(res = 2, filter = "filter")
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
#' @param triangulator LASRpipeline. A 'triangulate' algorithm.
#' @param operator string. '-' and '+' are supported.
#' @param store_in_attribute numeric. Use an extra bytes attribute to store the result.
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#'
#' # There is no normalize algorithm in lasR. Let's create one
#' normalize = function() {
#'   mesh  <- triangulate(filter = "-keep_class 2")
#'   norm <- mesh + transform_with_triangulation(mesh)
#'   return(norm)
#'  }
#'
#' pipeline <- reader(f) + normalize() + write_las()
#' ans <- processor(pipeline)
#'
#' @seealso
#' \link{reader}
#' \link{triangulate}
#' \link{write_las}
#' @export
transform_with_triangulation = function(triangulator, operator = "-", store_in_attribute = "")
{
  if (!methods::is(triangulator, "LASRpipeline")) stop("triangulator must be a 'LASRpipeline'")
  if (length(triangulator) != 1L) stop("cannot input a complex pipeline")
  if (triangulator[[1]]$algoname != "triangulate") stop("the algorithm must be 'triangulate'")
  if (methods::is(triangulator, "LASRpipeline"))

  ans <- list(algoname = "transform_with_triangulation", connect = triangulator[[1]][["uid"]], operator = operator, store_in_attribute = store_in_attribute)
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
#' normalize <- tri + transform_with_triangulation(tri)
#' pipeline <- read + normalize + write_las(paste0(tempdir(), "/*_norm.las"))
#' processor(pipeline)
#' @export
write_las = function(ofile = paste0(tempdir(), "/*.las"), filter = "", keep_buffer = FALSE)
{
  ans <- list(algoname = "write_las", filter = filter, output = ofile, keep_buffer = keep_buffer)
  set_lasr_class(ans)
}

#' Write LAX files
#'
#' Write a lax file from a las or laz file. A lax file is a tiny file which can come with a las or laz
#' and which spatially index the data to make faster spatial queries. It has been created by Martin
#' Isenburg in LASlib. lasR supports lax files and enable to write a lax. For more options, use
#' lasindex from LAStools (for more informations see references)
#'
#' @references
#' https://rapidlasso.com/
#' https://rapidlasso.com/2012/12/03/lasindex-spatial-indexing-of-lidar-data/
#' https://github.com/LAStools/LAStools
#'
#' @examples
#' f <- system.file("extdata", "Topography.las", package="lasR")
#' processor(reader(f) + write_lax())
#' @noRd
write_lax = function()
{
  ans <- list(algoname = "write_lax")
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


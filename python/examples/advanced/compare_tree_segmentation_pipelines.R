suppressPackageStartupMessages({
  library(sf)
  library(terra)
})

sf::sf_use_s2(FALSE)

script_arg <- grep("^--file=", commandArgs(FALSE), value = TRUE)
script_path <- if (length(script_arg)) {
  normalizePath(sub("^--file=", "", script_arg[[1]]))
} else {
  normalizePath("benchmarks/copc/compare_tree_segmentation_pipelines.R")
}

script_dir <- dirname(script_path)
# script_dir = <repo>/python/examples/advanced, so repo_dir is three levels up.
repo_dir <- normalizePath(file.path(script_dir, "..", "..", ".."))

input_las <- Sys.getenv(
  "LASR_TREE_INPUT",
  unset = file.path(script_dir, "data", "input.laz")
)
out_dir <- Sys.getenv("LASR_COMPARE_DIR", unset = "/tmp/lasr_tree_compare")
run_pipelines <- tolower(Sys.getenv("LASR_COMPARE_RUN_PIPELINES", unset = "true")) %in%
  c("1", "true", "yes", "y")

r_dir <- file.path(out_dir, "r")
py_dir <- file.path(out_dir, "py")
dir.create(r_dir, recursive = TRUE, showWarnings = FALSE)
dir.create(py_dir, recursive = TRUE, showWarnings = FALSE)

r_outputs <- list(
  copc = file.path(r_dir, "input_treeid.copc.laz"),
  tops = file.path(r_dir, "tree_tops.fgb"),
  crowns = file.path(r_dir, "tree_crowns.fgb")
)
py_outputs <- list(
  copc = file.path(py_dir, "input_treeid.copc.laz"),
  tops = file.path(py_dir, "tree_tops.fgb"),
  crowns = file.path(py_dir, "tree_crowns.fgb")
)

env_var <- function(name, value) paste0(name, "=", value)

run_checked <- function(command, args, env = character()) {
  status <- system2(command, args = args, env = env)
  if (!identical(status, 0L)) {
    stop(command, " failed with status ", status, call. = FALSE)
  }
}

# Force sequential parity unless the caller explicitly overrides the strategy,
# so the comparison stays apples-to-apples by default.
parallel_mode <- Sys.getenv("LASR_TREE_PARALLEL", unset = "sequential")

pipeline_env <- function(outputs) {
  vars <- c(
    env_var("LASR_TREE_INPUT", input_las),
    env_var("LASR_TREE_OUT_COPC", outputs$copc),
    env_var("LASR_TREE_OUT_TOPS", outputs$tops),
    env_var("LASR_TREE_OUT_CROWNS", outputs$crowns),
    env_var("LASR_TREE_PARALLEL", parallel_mode)
  )
  # Forward optional tuning parameters only when set, so each pipeline falls
  # back to its own default otherwise.
  for (name in c("LASR_TREE_MIN_HEIGHT", "LASR_TREE_WINDOW_SIZE",
                 "LASR_TREE_CHM_RES",   "LASR_TREE_MAX_CR",
                 "LASR_TREE_AOI",       "LASR_TREE_AOI_DEPTH")) {
    val <- Sys.getenv(name, unset = "")
    if (nzchar(val)) vars <- c(vars, env_var(name, val))
  }
  vars
}

if (run_pipelines) {
  message("Running R tree segmentation pipeline...")
  run_checked(
    file.path(R.home("bin"), "Rscript"),
    file.path(script_dir, "tree_segmentation_pipeline.R"),
    env = pipeline_env(r_outputs)
  )

  message("Running pylasr tree segmentation pipeline...")
  py_env <- c(
    env_var("PYTHONPATH", file.path(repo_dir, "python")),
    pipeline_env(py_outputs),
    env_var("LASR_TREE_SCRATCH_DIR", py_dir),
    env_var("LASR_TREE_SCRATCH_PREFIX", "pylasr_compare")
  )
  run_checked(
    "python3",
    file.path(script_dir, "tree_segmentation_pipeline.py"),
    env = py_env
  )
}

read_vector <- function(path) {
  if (!file.exists(path)) stop("Missing vector output: ", path, call. = FALSE)
  st_read(path, quiet = TRUE)
}

# Read (X, Y, treeID) from a labeled COPC via lasR's callback. Reusing the
# same reader as the pipelines avoids adding an rlas/pdal dependency for the
# comparison.
read_treeids <- function(path) {
  if (!file.exists(path)) stop("Missing COPC output: ", path, call. = FALSE)
  # expose = "xyzE" gives X/Y/Z plus all extrabytes (treeID).
  # no_las_update keeps the returned data.frame as a *result* instead of
  # treating it as a point-cloud replacement. When the pipeline has a single
  # result-producing stage, exec() returns the data.frame directly (not
  # wrapped in a named list), so accept either shape.
  pipeline <- lasR::reader() + lasR::callback(
    function(data) data.frame(X = data$X, Y = data$Y, treeID = data$treeID),
    expose = "xyzE",
    no_las_update = TRUE
  )
  ans <- lasR::exec(pipeline, on = path)
  if (is.data.frame(ans)) ans else ans$callback
}

numeric_summary <- function(x) {
  x <- x[is.finite(x)]
  c(
    min = min(x),
    median = median(x),
    mean = mean(x),
    max = max(x)
  )
}

nearest_summary <- function(from, to, label) {
  nearest <- st_nearest_feature(from, to)
  distances <- as.numeric(st_distance(st_geometry(from), st_geometry(to)[nearest], by_element = TRUE))
  h_delta <- abs(from$H - to$H[nearest])
  matched_1m <- distances <= 1

  if (any(matched_1m)) {
    h_median <- median(h_delta[matched_1m], na.rm = TRUE)
    h_p95 <- unname(quantile(h_delta[matched_1m], 0.95, na.rm = TRUE))
  } else {
    h_median <- NA_real_
    h_p95 <- NA_real_
  }

  data.frame(
    direction = label,
    n = nrow(from),
    within_0_25m = sum(distances <= 0.25),
    within_0_5m = sum(distances <= 0.5),
    within_1m = sum(distances <= 1),
    within_2m = sum(distances <= 2),
    median_distance_m = median(distances),
    p95_distance_m = unname(quantile(distances, 0.95)),
    median_abs_h_delta_1m = h_median,
    p95_abs_h_delta_1m = h_p95,
    check.names = FALSE
  )
}

r_tops <- read_vector(r_outputs$tops)
py_tops <- read_vector(py_outputs$tops)
r_crowns <- read_vector(r_outputs$crowns)
py_crowns <- read_vector(py_outputs$crowns)

st_crs(r_tops) <- NA
st_crs(py_tops) <- NA
st_crs(r_crowns) <- NA
st_crs(py_crowns) <- NA

# Per-point treeID parity: join the two clouds by rounded (X, Y) and compare
# treeID label-by-label. Both pipelines write to the same grid, so a 0.01 m
# (cm) rounding matches the LAS scale and avoids floating-point misses.
r_pts  <- read_treeids(r_outputs$copc)
py_pts <- read_treeids(py_outputs$copc)
xy_key <- function(df) paste(round(df$X, 2), round(df$Y, 2), sep = ":")
r_pts$key  <- xy_key(r_pts)
py_pts$key <- xy_key(py_pts)
joined <- merge(
  r_pts[, c("key", "treeID")],
  py_pts[, c("key", "treeID")],
  by = "key", suffixes = c("_R", "_py")
)
n_points_r  <- nrow(r_pts)
n_points_py <- nrow(py_pts)
n_joined    <- nrow(joined)
n_match     <- if (n_joined) sum(joined$treeID_R == joined$treeID_py) else 0L

r_areas <- as.numeric(st_area(r_crowns))
py_areas <- as.numeric(st_area(py_crowns))

count_table <- data.frame(
  metric = c(
    "tops",
    "unique top treeID",
    "crowns",
    "labeled points",
    "labeled points > 0",
    "COPC size bytes"
  ),
  R = c(
    nrow(r_tops),
    length(unique(r_tops$treeID)),
    nrow(r_crowns),
    n_points_r,
    sum(r_pts$treeID > 0L),
    file.info(r_outputs$copc)$size
  ),
  pylasr = c(
    nrow(py_tops),
    length(unique(py_tops$treeID)),
    nrow(py_crowns),
    n_points_py,
    sum(py_pts$treeID > 0L),
    file.info(py_outputs$copc)$size
  )
)
count_table$delta <- count_table$pylasr - count_table$R
count_table$relative_delta <- count_table$delta / count_table$R

height_table <- rbind(
  data.frame(pipeline = "R", field = "H", t(numeric_summary(r_tops$H))),
  data.frame(pipeline = "pylasr", field = "H", t(numeric_summary(py_tops$H))),
  data.frame(pipeline = "R", field = "Z", t(numeric_summary(r_tops$Z))),
  data.frame(pipeline = "pylasr", field = "Z", t(numeric_summary(py_tops$Z))),
  data.frame(pipeline = "R", field = "crown_area", t(numeric_summary(r_areas))),
  data.frame(pipeline = "pylasr", field = "crown_area", t(numeric_summary(py_areas)))
)

nearest_table <- rbind(
  nearest_summary(r_tops, py_tops, "R top -> nearest pylasr top"),
  nearest_summary(py_tops, r_tops, "pylasr top -> nearest R top")
)

point_table <- data.frame(
  metric = c(
    "joined_points",
    "treeID_equal_on_joined",
    "fraction_treeID_equal"
  ),
  value = c(
    as.character(n_joined),
    as.character(n_match),
    if (n_joined) sprintf("%.6f", n_match / n_joined) else "NA"
  )
)

common_top_ids <- intersect(r_tops$treeID, py_tops$treeID)
common_crown_ids <- intersect(r_crowns$treeID, py_crowns$treeID)
id_table <- data.frame(
  metric = c("common top treeID", "common crown treeID"),
  value = c(length(common_top_ids), length(common_crown_ids))
)

report <- c(
  "R vs pylasr Tree Segmentation Comparison",
  paste("Input:", input_las),
  paste("Output directory:", out_dir),
  "",
  "Counts",
  capture.output(print(count_table, row.names = FALSE, digits = 4)),
  "",
  "Attribute Summaries",
  capture.output(print(height_table, row.names = FALSE, digits = 4)),
  "",
  "Nearest Top Matching",
  capture.output(print(nearest_table, row.names = FALSE, digits = 4)),
  "",
  "Per-Point treeID Comparison (COPC)",
  capture.output(print(point_table, row.names = FALSE, digits = 6)),
  "",
  "Direct ID Overlap",
  capture.output(print(id_table, row.names = FALSE, digits = 4))
)

report_path <- file.path(out_dir, "comparison_summary.txt")
writeLines(report, report_path)
writeLines(report)
message("Wrote comparison report: ", report_path)

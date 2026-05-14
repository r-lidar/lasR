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
repo_dir <- normalizePath(file.path(script_dir, "..", ".."))

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
  las = file.path(r_dir, "input_treeid.laz"),
  tops = file.path(r_dir, "tree_tops.fgb"),
  crowns = file.path(r_dir, "tree_crowns.fgb"),
  dtm = file.path(r_dir, "tree_segmentation_dtm.tif"),
  tree = file.path(r_dir, "tree_segmentation_treeid.tif")
)
py_outputs <- list(
  las = file.path(py_dir, "input_treeid.laz"),
  tops = file.path(py_dir, "tree_tops.fgb"),
  crowns = file.path(py_dir, "tree_crowns.fgb"),
  dtm = file.path(py_dir, "tree_segmentation_dtm.tif"),
  tree = file.path(py_dir, "tree_segmentation_treeid.tif")
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
    env_var("LASR_TREE_OUT_LAS", outputs$las),
    env_var("LASR_TREE_OUT_TOPS", outputs$tops),
    env_var("LASR_TREE_OUT_CROWNS", outputs$crowns),
    env_var("LASR_TREE_OUT_DTM_RASTER", outputs$dtm),
    env_var("LASR_TREE_OUT_TREE_RASTER", outputs$tree),
    env_var("LASR_TREE_PARALLEL", parallel_mode)
  )
  # Forward optional tuning parameters only when set, so each pipeline falls
  # back to its own default otherwise.
  for (name in c("LASR_TREE_MIN_HEIGHT", "LASR_TREE_WINDOW_SIZE",
                 "LASR_TREE_DTM_RES",   "LASR_TREE_CHM_RES",
                 "LASR_TREE_MAX_CR")) {
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

read_tree_raster <- function(path) {
  if (!file.exists(path)) stop("Missing raster output: ", path, call. = FALSE)
  rast(path)
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

r_tree <- read_tree_raster(r_outputs$tree)
py_tree <- read_tree_raster(py_outputs$tree)
r_dtm <- read_tree_raster(r_outputs$dtm)
py_dtm <- read_tree_raster(py_outputs$dtm)
same_grid <- compareGeom(r_tree, py_tree, stopOnError = FALSE)
if (!same_grid) {
  py_tree <- resample(py_tree, r_tree, method = "near")
}

same_dtm_grid <- compareGeom(r_dtm, py_dtm, stopOnError = FALSE)
if (!same_dtm_grid) {
  py_dtm <- resample(py_dtm, r_dtm, method = "bilinear")
}

r_values <- values(r_tree, mat = FALSE)
py_values <- values(py_tree, mat = FALSE)
r_dtm_values <- values(r_dtm, mat = FALSE)
py_dtm_values <- values(py_dtm, mat = FALSE)
r_mask <- !is.na(r_values) & r_values > 0
py_mask <- !is.na(py_values) & py_values > 0
overlap <- r_mask & py_mask
union <- r_mask | py_mask
dtm_overlap <- is.finite(r_dtm_values) & is.finite(py_dtm_values)
dtm_delta <- py_dtm_values[dtm_overlap] - r_dtm_values[dtm_overlap]

r_areas <- as.numeric(st_area(r_crowns))
py_areas <- as.numeric(st_area(py_crowns))

count_table <- data.frame(
  metric = c(
    "tops",
    "unique top treeID",
    "crowns",
    "unique raster treeID",
    "raster crown cells",
    "LAZ size bytes"
  ),
  R = c(
    nrow(r_tops),
    length(unique(r_tops$treeID)),
    nrow(r_crowns),
    length(unique(r_values[r_mask])),
    sum(r_mask),
    file.info(r_outputs$las)$size
  ),
  pylasr = c(
    nrow(py_tops),
    length(unique(py_tops$treeID)),
    nrow(py_crowns),
    length(unique(py_values[py_mask])),
    sum(py_mask),
    file.info(py_outputs$las)$size
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

raster_table <- data.frame(
  metric = c(
    "same_grid",
    "mask_intersection_cells",
    "mask_union_cells",
    "mask_iou",
    "raw_treeID_equal_on_overlap"
  ),
  value = c(
    as.character(same_grid),
    as.character(sum(overlap)),
    as.character(sum(union)),
    sprintf("%.6f", sum(overlap) / sum(union)),
    sprintf("%.6f", mean(r_values[overlap] == py_values[overlap], na.rm = TRUE))
  )
)

dtm_table <- data.frame(
  metric = c(
    "same_grid",
    "overlap_cells",
    "median_delta_pylasr_minus_R",
    "mean_delta_pylasr_minus_R",
    "median_abs_delta",
    "p95_abs_delta",
    "max_abs_delta"
  ),
  value = c(
    as.character(same_dtm_grid),
    as.character(sum(dtm_overlap)),
    sprintf("%.6f", median(dtm_delta, na.rm = TRUE)),
    sprintf("%.6f", mean(dtm_delta, na.rm = TRUE)),
    sprintf("%.6f", median(abs(dtm_delta), na.rm = TRUE)),
    sprintf("%.6f", unname(quantile(abs(dtm_delta), 0.95, na.rm = TRUE))),
    sprintf("%.6f", max(abs(dtm_delta), na.rm = TRUE))
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
  "Raster Mask Comparison",
  capture.output(print(raster_table, row.names = FALSE, digits = 6)),
  "",
  "DTM Comparison",
  capture.output(print(dtm_table, row.names = FALSE, digits = 6)),
  "",
  "Direct ID Overlap",
  capture.output(print(id_table, row.names = FALSE, digits = 4))
)

report_path <- file.path(out_dir, "comparison_summary.txt")
writeLines(report, report_path)
writeLines(report)
message("Wrote comparison report: ", report_path)

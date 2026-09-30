suppressPackageStartupMessages({
  library(terra)
  library(lasR)
  library(sf)
})

script_arg <- grep("^--file=", commandArgs(FALSE), value = TRUE)
script_dir <- if (length(script_arg)) {
  dirname(normalizePath(sub("^--file=", "", script_arg[[1]])))
} else {
  getwd()
}
repo_root <- normalizePath(file.path(script_dir, "..", ".."), mustWork = FALSE)
default_input <- file.path(repo_root, "benchmarks", "copc", "data", "input.laz")

env_double <- function(name, default) {
  raw <- Sys.getenv(name, unset = "")
  if (!nzchar(raw)) return(default)
  v <- suppressWarnings(as.numeric(raw))
  if (is.na(v)) stop(sprintf("Env %s must be numeric, got %s", name, raw), call. = FALSE)
  v
}
parallel_mode <- tolower(Sys.getenv("LASR_TREE_PARALLEL", unset = "sequential"))
parallel_strategy <- switch(parallel_mode,
  "sequential"        = sequential(),
  "concurrent-points" = concurrent_points(half_cores()),
  "concurrent-files"  = concurrent_files(half_cores()),
  stop(sprintf(
    "LASR_TREE_PARALLEL must be sequential|concurrent-points|concurrent-files, got %s",
    parallel_mode
  ), call. = FALSE)
)

cfg <- list(
  input = Sys.getenv("LASR_TREE_INPUT", unset = default_input),
  out_las = Sys.getenv("LASR_TREE_OUT_LAS", unset = "/tmp/input_treeid.laz"),
  out_tops = Sys.getenv("LASR_TREE_OUT_TOPS", unset = "/tmp/tree_tops.fgb"),
  out_crowns = Sys.getenv("LASR_TREE_OUT_CROWNS", unset = "/tmp/tree_crowns.fgb"),
  out_dtm_raster = Sys.getenv("LASR_TREE_OUT_DTM_RASTER", unset = ""),
  out_tree_raster = Sys.getenv("LASR_TREE_OUT_TREE_RASTER", unset = ""),
  min_height = env_double("LASR_TREE_MIN_HEIGHT", 2),    # tree-top / region-growing height threshold (m)
  window_size = env_double("LASR_TREE_WINDOW_SIZE", 5),  # local-maximum search window diameter (m)
  dtm_res = env_double("LASR_TREE_DTM_RES", 0.5),        # DTM raster resolution (m)
  chm_res = env_double("LASR_TREE_CHM_RES", 1),          # CHM raster resolution (m); used as window size too
  max_cr = env_double("LASR_TREE_MAX_CR", 10),           # max crown diameter for region_growing (m)
  parallel_strategy = parallel_strategy
)

for (path in c(cfg$out_las, cfg$out_tops, cfg$out_crowns, cfg$out_dtm_raster, cfg$out_tree_raster)) {
  if (nzchar(path)) {
    dir.create(dirname(path), recursive = TRUE, showWarnings = FALSE)
  }
}

extract_values <- function(raster, xy, method = "simple") {
  terra::extract(raster, as.matrix(xy), method = method)[[1]]
}

pick_raster_by_res <- function(rasters, target_res, tolerance = 1e-9) {
  res_vec <- vapply(rasters, function(r) terra::res(r)[1], numeric(1))
  matches <- which(abs(res_vec - target_res) <= tolerance)

  if (length(matches) != 1L) {
    stop(
      "Expected exactly one raster at resolution ", target_res,
      "; found ", length(matches), ". Available resolutions: ",
      paste(res_vec, collapse = ", "),
      call. = FALSE
    )
  }

  rasters[[matches]]
}

# --- Pipeline A: ground TIN -> 0.5 m DTM -> HAG -> 1 m CHM, point-LM, Dalponte
tri    <- triangulate(filter = keep_ground_and_water())
hag_eb <- add_extrabytes("double", "HAG", "Height Above Ground")
hag    <- transform_with(tri, store_in_attribute = "HAG")
dtm    <- rasterize(cfg$dtm_res, tri)
chm    <- rasterize(cfg$chm_res, "HAG_max")
lmx    <- local_maximum(
  ws = cfg$window_size,
  min_height = cfg$min_height,
  filter = sprintf("HAG > %g", cfg$min_height),
  use_attribute = "HAG",
  record_attributes = TRUE
)
tree   <- region_growing(chm, lmx, th_tree = cfg$min_height, max_cr = cfg$max_cr)

# Rasterize the DTM before transform_with(): the triangulation stores point
# indices, and transform_with can compact/delete points outside the TIN.
ansA <- exec(reader() + tri + dtm + hag_eb + hag + chm + lmx + tree, on = cfg$input,
             ncores = cfg$parallel_strategy, buffer = 0, chunk = 0)

rasters   <- ansA[vapply(ansA, inherits, logical(1), "SpatRaster")]
dtm_rast  <- pick_raster_by_res(rasters, cfg$dtm_res)
tree_rast <- ansA$region_growing

if (nzchar(cfg$out_dtm_raster)) {
  terra::writeRaster(dtm_rast, cfg$out_dtm_raster, overwrite = TRUE)
}

if (nzchar(cfg$out_tree_raster)) {
  terra::writeRaster(tree_rast, cfg$out_tree_raster, overwrite = TRUE)
}

# --- Tree tops with HAG, falling back to the 0.5 m DTM when HAG is not recorded
tops <- ansA$local_maximum
xy   <- sf::st_coordinates(tops)[, c("X", "Y"), drop = FALSE]
ztop <- sf::st_coordinates(tops)[, "Z"]

tops$Z <- ztop
if ("HAG" %in% names(tops)) {
  tops$H <- as.numeric(tops$HAG)
  tops$ground <- tops$Z - tops$H
} else {
  tops$ground <- extract_values(dtm_rast, xy, method = "bilinear")
  missing_ground <- is.na(tops$ground)

  if (any(missing_ground)) {
    tops$ground[missing_ground] <- extract_values(dtm_rast, xy[missing_ground, ], method = "simple")
  }

  tops$H <- tops$Z - tops$ground
}

tops$treeID <- as.integer(extract_values(tree_rast, xy))
tops        <- tops[!is.na(tops$treeID) & tops$treeID > 0L & !is.na(tops$H) & tops$H >= cfg$min_height, ]
tops        <- sf::st_zm(tops, drop = TRUE)
sf::st_crs(tops) <- terra::crs(tree_rast)
tops        <- tops[, c("treeID", "Z", "ground", "H")]

sf::st_write(tops, cfg$out_tops, driver = "FlatGeobuf", quiet = TRUE, delete_dsn = TRUE)

# --- Crown polygons inherit H/Z of their seed apex --------------------------
crowns <- sf::st_as_sf(terra::as.polygons(tree_rast, dissolve = TRUE, na.rm = TRUE))
names(crowns)[1] <- "treeID"
crowns$treeID <- as.integer(crowns$treeID)
crowns <- crowns[crowns$treeID > 0L, ]

apices <- sf::st_drop_geometry(tops)[, c("treeID", "H", "Z")]
apices <- apices[order(apices$treeID, -apices$H, -apices$Z), ]
apices <- apices[!duplicated(apices$treeID), ]

crowns <- merge(
  crowns,
  apices,
  by = "treeID",
  all = FALSE
)

sf::st_write(crowns, cfg$out_crowns, driver = "FlatGeobuf", quiet = TRUE, delete_dsn = TRUE)

# --- Pipeline B: per-point treeID extrabyte ---------------------------------
assign_tree_id <- function(data) {
  ids <- extract_values(tree_rast, cbind(data$X, data$Y))
  ids[is.na(ids)] <- 0L
  data.frame(treeID = as.integer(ids))
}

if (file.exists(cfg$out_las)) unlink(cfg$out_las)

exec(
  reader() +
    add_extrabytes("int", "treeID", "tree segmentation ID") +
    callback(assign_tree_id, expose = "xy") +
    write_las(cfg$out_las),
  on = cfg$input,
  ncores = cfg$parallel_strategy, buffer = 0, chunk = 0
)

message("Tops: ", nrow(tops))
message("Crowns: ", nrow(crowns))
message("Wrote: ", cfg$out_las)
message("Wrote: ", cfg$out_tops)
message("Wrote: ", cfg$out_crowns)

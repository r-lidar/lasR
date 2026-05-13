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

env_bbox <- function(name) {
  raw <- Sys.getenv(name, unset = "")
  if (!nzchar(raw)) return(NULL)
  parts <- strsplit(raw, ",", fixed = TRUE)[[1]]
  if (length(parts) != 4L) {
    stop(sprintf("Env %s must be 'xmin,ymin,xmax,ymax', got %s", name, raw), call. = FALSE)
  }
  vals <- suppressWarnings(as.numeric(parts))
  if (any(is.na(vals))) {
    stop(sprintf("Env %s values must be numeric, got %s", name, raw), call. = FALSE)
  }
  if (!(vals[1] < vals[3] && vals[2] < vals[4])) {
    stop(sprintf("Env %s requires xmin<xmax and ymin<ymax, got %s", name, raw), call. = FALSE)
  }
  vals
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

aoi_bbox  <- env_bbox("LASR_TREE_AOI")
aoi_depth <- as.integer(Sys.getenv("LASR_TREE_AOI_DEPTH", unset = "-1"))

cfg <- list(
  input = Sys.getenv("LASR_TREE_INPUT", unset = default_input),
  out_copc = Sys.getenv("LASR_TREE_OUT_COPC", unset = "/tmp/input_treeid.copc.laz"),
  out_tops = Sys.getenv("LASR_TREE_OUT_TOPS", unset = "/tmp/tree_tops.fgb"),
  out_crowns = Sys.getenv("LASR_TREE_OUT_CROWNS", unset = "/tmp/tree_crowns.fgb"),
  out_tree_raster = Sys.getenv("LASR_TREE_OUT_TREE_RASTER", unset = ""),
  min_height = env_double("LASR_TREE_MIN_HEIGHT", 2),    # tree-top / region-growing height threshold (m)
  window_size = env_double("LASR_TREE_WINDOW_SIZE", 5),  # local-maximum search window diameter (m)
  chm_res = env_double("LASR_TREE_CHM_RES", 1),          # CHM raster resolution (m); used as window size too
  max_cr = env_double("LASR_TREE_MAX_CR", 10),           # max crown diameter for region_growing (m)
  parallel_strategy = parallel_strategy,
  aoi_bbox = aoi_bbox,
  aoi_depth = aoi_depth
)

# Use reader_rectangles() when an AOI is set; reader() otherwise. This keeps the
# pipeline shape identical for full-file vs AOI runs (EPT/COPC alike).
build_reader <- function() {
  if (is.null(cfg$aoi_bbox)) return(reader())
  reader_rectangles(
    xmin = cfg$aoi_bbox[1], ymin = cfg$aoi_bbox[2],
    xmax = cfg$aoi_bbox[3], ymax = cfg$aoi_bbox[4],
    depth = cfg$aoi_depth
  )
}

for (path in c(cfg$out_copc, cfg$out_tops, cfg$out_crowns, cfg$out_tree_raster)) {
  if (nzchar(path)) {
    dir.create(dirname(path), recursive = TRUE, showWarnings = FALSE)
  }
}

extract_values <- function(raster, xy, method = "simple") {
  terra::extract(raster, as.matrix(xy), method = method)[[1]]
}

# Single-pipeline segmentation: ground TIN -> HAG -> CHM -> point-LM ->
# Dalponte region_growing -> treeID per point -> drop HAG -> write COPC.
# The treeID per point is assigned via transform_with("=") sampling the
# region_growing raster, eliminating the separate callback-based Pipeline B.
tri      <- triangulate(filter = keep_ground_and_water())
hag_eb   <- add_extrabytes("double", "HAG", "Height Above Ground")
hag      <- transform_with(tri, store_in_attribute = "HAG")
chm      <- rasterize(cfg$chm_res, "HAG_max")
lmx      <- local_maximum(
  ws = cfg$window_size,
  min_height = cfg$min_height,
  filter = sprintf("HAG > %g", cfg$min_height),
  use_attribute = "HAG",
  record_attributes = TRUE
)
tree     <- region_growing(chm, lmx, th_tree = cfg$min_height, max_cr = cfg$max_cr)
tid_eb   <- add_extrabytes("int", "treeID", "tree segmentation ID")
tid_set  <- transform_with(tree, operator = "=", store_in_attribute = "treeID", bilinear = FALSE)
drop_hag <- remove_attribute("HAG")
writer   <- write_copc(cfg$out_copc)

if (file.exists(cfg$out_copc)) unlink(cfg$out_copc)

ansA <- exec(
  build_reader() + tri + hag_eb + hag + chm + lmx + tree +
    tid_eb + tid_set + drop_hag + writer,
  on = cfg$input,
  ncores = cfg$parallel_strategy, buffer = 0, chunk = 0
)

tree_rast <- ansA$region_growing

if (nzchar(cfg$out_tree_raster)) {
  terra::writeRaster(tree_rast, cfg$out_tree_raster, overwrite = TRUE)
}

# --- Tree tops: HAG is carried as a recorded attribute on each LM feature
tops <- ansA$local_maximum
xy   <- sf::st_coordinates(tops)[, c("X", "Y"), drop = FALSE]
ztop <- sf::st_coordinates(tops)[, "Z"]

tops$Z <- ztop
tops$H <- as.numeric(tops$HAG)
tops$ground <- tops$Z - tops$H

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

message("Tops: ", nrow(tops))
message("Crowns: ", nrow(crowns))
message("Wrote: ", cfg$out_copc)
message("Wrote: ", cfg$out_tops)
message("Wrote: ", cfg$out_crowns)

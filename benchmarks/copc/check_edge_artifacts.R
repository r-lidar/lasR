#!/usr/bin/env Rscript
# Edge-artifact checker for a merged multi-tile COPC.
# Usage: Rscript check_edge_artifacts.R <merged_copc> <tiles_dir> <out_dir> [res_m]
#
# Strategy:
#   1. Parse each source tile's bounding box from its LAS header.
#   2. Build a point-density raster of the merged COPC.
#   3. Identify internal tile boundaries (seams between tiles).
#   4. For each seam, sample a ±STRIP_W metre strip and compare its median
#      density to an interior reference.  Flag GAP (<50 %) or SPIKE (>180 %).
#   5. Render the density map with seam lines overlaid as PNG.
#   6. Repeat the seam comparison per octree depth (seam_stats_by_depth.csv).
#   7. Compare the point count with the tile headers and count duplicated
#      points on the seams (seam_duplicates.csv).
options(error = function() { traceback(2); quit(status = 1) })
suppressPackageStartupMessages({
  library(lasR)
  library(terra)
})

args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 3L)
  stop("usage: check_edge_artifacts.R <merged_copc> <tiles_dir> <out_dir> [res_m]")
merged_copc <- args[[1L]]
tiles_dir   <- args[[2L]]
out_dir     <- args[[3L]]
res_m       <- if (length(args) >= 4L) as.numeric(args[[4L]]) else 5

stopifnot(file.exists(merged_copc))
dir.create(out_dir, showWarnings = FALSE, recursive = TRUE)

STRIP_W        <- 30   # metres either side of a seam to call "boundary zone"
INTERIOR_MARGIN <- 80  # metres inward from every edge for interior reference

# ---- Parse tile bounding boxes from LAS headers (pure Python) ----------------
tile_files <- list.files(tiles_dir, pattern = "(?i)\\.la[sz]$",
                         full.names = TRUE, recursive = FALSE)
if (!length(tile_files)) stop(sprintf("no LAZ/LAS files in: %s", tiles_dir))

py_script <- '
import struct, sys
with open(sys.argv[1], "rb") as f:
    b = f.read(375)
assert b[0:4] == b"LASF", "not LAS/LAZ"
xmax, xmin = struct.unpack("<dd", b[179:195])
ymax, ymin = struct.unpack("<dd", b[195:211])
n = struct.unpack("<Q", b[247:255])[0] or struct.unpack("<I", b[107:111])[0]
print(xmin, xmax, ymin, ymax, n)
'

get_bbox <- function(path) {
  out <- tryCatch(
    system2("python3", c("-", shQuote(path)), input = py_script,
            stdout = TRUE, stderr = FALSE),
    error = function(e) character(0))
  if (!length(out)) return(NULL)
  v <- as.numeric(strsplit(trimws(out), " ")[[1]])
  if (length(v) != 5L || anyNA(v)) return(NULL)
  list(xmin = v[1], xmax = v[2], ymin = v[3], ymax = v[4], n = v[5])
}

cat("[check_edge_artifacts] tile bounding boxes:\n")
bboxes <- Filter(Negate(is.null), lapply(tile_files, get_bbox))
names(bboxes) <- basename(tile_files)[!vapply(lapply(tile_files, get_bbox), is.null, logical(1))]

if (!length(bboxes)) stop("could not parse any tile bounding boxes")

for (nm in names(bboxes)) {
  bb <- bboxes[[nm]]
  cat(sprintf("  %s  X[%.1f, %.1f]  Y[%.1f, %.1f]\n",
              nm, bb$xmin, bb$xmax, bb$ymin, bb$ymax))
}

# ---- Density raster of merged COPC -------------------------------------------
cat(sprintf("[check_edge_artifacts] computing density raster (res=%gm)...\n", res_m))
pipeline_dens <- reader() + lasR::rasterize(res_m, "count")
dens <- exec(pipeline_dens, on = merged_copc, progress = FALSE)
cat(sprintf("[check_edge_artifacts] raster: %d x %d cells, extent %s\n",
            nrow(dens), ncol(dens), as.character(ext(dens))))

# ---- Identify internal seams -------------------------------------------------
all_x <- sort(unique(unlist(lapply(bboxes, function(b) c(b$xmin, b$xmax)))))
all_y <- sort(unique(unlist(lapply(bboxes, function(b) c(b$ymin, b$ymax)))))

xmin_all <- min(unlist(lapply(bboxes, `[[`, "xmin")))
xmax_all <- max(unlist(lapply(bboxes, `[[`, "xmax")))
ymin_all <- min(unlist(lapply(bboxes, `[[`, "ymin")))
ymax_all <- max(unlist(lapply(bboxes, `[[`, "ymax")))

seam_x <- all_x[all_x > xmin_all & all_x < xmax_all]
seam_y <- all_y[all_y > ymin_all & all_y < ymax_all]

cat(sprintf("[check_edge_artifacts] internal X seams: %s\n",
            if (length(seam_x)) paste(round(seam_x), collapse = ", ") else "none"))
cat(sprintf("[check_edge_artifacts] internal Y seams: %s\n",
            if (length(seam_y)) paste(round(seam_y), collapse = ", ") else "none"))

# ---- Interior reference density ----------------------------------------------
e <- ext(dens)
interior_ext <- ext(
  max(xmin_all + INTERIOR_MARGIN, as.numeric(e$xmin)),
  min(xmax_all - INTERIOR_MARGIN, as.numeric(e$xmax)),
  max(ymin_all + INTERIOR_MARGIN, as.numeric(e$ymin)),
  min(ymax_all - INTERIOR_MARGIN, as.numeric(e$ymax))
)
interior_dens <- tryCatch(crop(dens, interior_ext), error = function(e2) dens)
interior_vals <- as.vector(values(interior_dens, na.rm = TRUE))
interior_vals <- interior_vals[interior_vals > 0]
interior_med  <- if (length(interior_vals)) median(interior_vals) else NA_real_
cat(sprintf("[check_edge_artifacts] interior reference median density: %.2f pts/%gm²\n",
            interior_med, res_m))

# ---- Seam strip analysis -----------------------------------------------------
strip_stats <- function(axis, coord) {
  e2 <- ext(dens)
  crop_ext <- if (axis == "x") {
    ext(coord - STRIP_W, coord + STRIP_W, as.numeric(e2$ymin), as.numeric(e2$ymax))
  } else {
    ext(as.numeric(e2$xmin), as.numeric(e2$xmax), coord - STRIP_W, coord + STRIP_W)
  }
  strip <- tryCatch(crop(dens, crop_ext), error = function(e3) NULL)
  if (is.null(strip)) return(NULL)
  v <- as.vector(values(strip, na.rm = TRUE))
  v <- v[v > 0]
  list(n = length(v),
       mean   = if (length(v)) mean(v)   else NA,
       median = if (length(v)) median(v) else NA,
       cv     = if (length(v) > 1 && mean(v) > 0) sd(v) / mean(v) else NA)
}

artifacts_found <- FALSE
results <- data.frame(axis = character(), coord = numeric(),
                      median = numeric(), ratio = numeric(),
                      flag = character(), stringsAsFactors = FALSE)

check_seam <- function(axis, coord) {
  s <- strip_stats(axis, coord)
  if (is.null(s)) return()
  ratio <- if (!is.na(interior_med) && interior_med > 0) s$median / interior_med else NA
  flag  <- if (is.na(ratio)) "?" else if (ratio < 0.5) "GAP" else if (ratio > 1.8) "SPIKE" else "ok"
  if (flag %in% c("GAP", "SPIKE")) artifacts_found <<- TRUE
  cat(sprintf("[check_edge_artifacts] %s seam @ %.1f: med=%.2f ratio=%.2f [%s]\n",
              toupper(axis), coord, s$median, ratio, flag))
  results[nrow(results) + 1L, ] <<- list(axis, coord, s$median, ratio, flag)
}

for (sx in seam_x) check_seam("x", sx)
for (sy in seam_y) check_seam("y", sy)

# ---- Save results table -------------------------------------------------------
if (nrow(results)) {
  write.csv(results, file.path(out_dir, "seam_stats.csv"), row.names = FALSE)
  cat(sprintf("[check_edge_artifacts] seam stats written to %s\n",
              file.path(out_dir, "seam_stats.csv")))
}

# ---- Density map PNG with seam lines -----------------------------------------
pal     <- hcl.colors(256, "YlOrRd")
out_png <- file.path(out_dir, "edge_artifact_check.png")
png(out_png, width = 1200, height = 1100, res = 120)
par(mar = c(4, 4, 3, 5))
vmax <- quantile(values(dens, na.rm = TRUE), 0.99, na.rm = TRUE)
plot(clamp(dens, 0, vmax),
     col   = pal,
     range = c(0, vmax),
     main  = sprintf("Merged COPC density (res=%gm) — seams in cyan\n%s",
                     res_m, basename(merged_copc)))
for (sx in seam_x) abline(v = sx, col = "cyan", lwd = 2, lty = 2)
for (sy in seam_y) abline(h = sy, col = "cyan", lwd = 2, lty = 2)
if (length(seam_x) || length(seam_y))
  legend("topleft", legend = "tile seam", col = "cyan", lty = 2, lwd = 2, bty = "n")
dev.off()
cat(sprintf("[check_edge_artifacts] wrote %s\n", out_png))

# ---- Per-depth seam profile ---------------------------------------------------
# The strip test above rasterizes every point, so its result does not depend on
# how the writer distributed points across octree levels: it can only show a
# gap or a pile-up of points at a seam. Tile-boundary artifacts in the LOD show
# up in the coarse levels, so repeat the comparison on the points visible at
# each depth. For each depth and seam, the mean density of the two cell
# rows/columns touching the seam (and of the ±STRIP_W strip) is expressed
# relative to the mean over the whole raster, next to the spread of all
# row/column means so the seam can be judged against ordinary variation.
# Informational only: it does not change the exit status.
snap     <- function(v) unique(round(v / res_m) * res_m)
seam_x_d <- snap(seam_x)
seam_y_d <- snap(seam_y)
by_depth <- data.frame()
if (length(seam_x_d) || length(seam_y_d)) {
  n_full <- sum(values(dens, na.rm = TRUE), na.rm = TRUE)
  tol    <- 0.05 * res_m
  for (d in 0:16) {
    r <- tryCatch(exec(reader(depth = d) + lasR::rasterize(res_m, "count"),
                       on = merged_copc, progress = FALSE),
                  error = function(e) NULL)
    if (is.null(r)) break
    m <- as.matrix(r, wide = TRUE)
    m[is.na(m)] <- 0
    n_depth <- sum(m)
    xs <- xFromCol(r, seq_len(ncol(r)))
    ys <- yFromRow(r, seq_len(nrow(r)))
    # Only cells that lie fully inside the union of the tiles.
    in_x <- xs - res_m / 2 >= xmin_all - tol & xs + res_m / 2 <= xmax_all + tol
    in_y <- ys - res_m / 2 >= ymin_all - tol & ys + res_m / 2 <= ymax_all + tol
    m  <- m[in_y, in_x, drop = FALSE]
    n  <- sum(m)
    profile_row <- function(axis, coord) {
      v   <- if (axis == "x") colMeans(m) else rowMeans(m)
      pos <- if (axis == "x") xs[in_x] else ys[in_y]
      rel <- v / mean(v)
      data.frame(depth = d, points = n, axis = axis, coord = coord,
                 seam_ratio  = mean(rel[abs(pos - coord) < res_m]),
                 strip_ratio = mean(rel[abs(pos - coord) < STRIP_W]),
                 profile_sd  = sd(rel), profile_min = min(rel), profile_max = max(rel))
    }
    for (sx in seam_x_d) by_depth <- rbind(by_depth, profile_row("x", sx))
    for (sy in seam_y_d) by_depth <- rbind(by_depth, profile_row("y", sy))
    if (n_depth >= n_full) break   # deepest level reached: every point is visible
  }
}
if (nrow(by_depth)) {
  by_depth_csv <- file.path(out_dir, "seam_stats_by_depth.csv")
  write.csv(by_depth, by_depth_csv, row.names = FALSE)
  cat("[check_edge_artifacts] per-depth seam profile (density relative to the depth's mean):\n")
  op <- options(width = 200)
  print(format(by_depth, digits = 3), row.names = FALSE)
  options(op)
  cat(sprintf("[check_edge_artifacts] per-depth seam stats written to %s\n", by_depth_csv))
}

# ---- Point count and duplicated points on the seams --------------------------
# Compare the merged point count with the sum of the tile headers, then count
# the points of the merged file within DUP_W of each seam that share X, Y, Z,
# gpstime and return number with another point. A point that lies exactly on a
# shared tile edge can be delivered twice when the tiles are read as one
# collection; the density tests above are far too coarse to see that.
# Informational only: it does not change the exit status.
DUP_W      <- 0.05
n_inputs   <- sum(vapply(bboxes, function(b) b$n, numeric(1)))
n_merged   <- sum(values(dens, na.rm = TRUE), na.rm = TRUE)
cat(sprintf("[check_edge_artifacts] points: tile headers %.0f, merged %.0f (difference %+.0f)\n",
            n_inputs, n_merged, n_merged - n_inputs))
count_dups <- function(xmin, ymin, xmax, ymax) {
  keep <- function(data) data
  ans  <- exec(reader_rectangles(xmin, ymin, xmax, ymax) +
                 callback(keep, expose = "xyztr", no_las_update = TRUE),
               on = merged_copc, progress = FALSE)
  d <- if (is.data.frame(ans)) ans else do.call(rbind, ans)
  if (is.null(d) || !nrow(d)) return(c(0, 0))
  key <- paste(d$X, d$Y, d$Z, d$gpstime, d$ReturnNumber)
  c(nrow(d), sum(duplicated(key)))
}
dups <- data.frame()
for (sx in seam_x_d) {
  v <- count_dups(sx - DUP_W, ymin_all, sx + DUP_W, ymax_all)
  dups <- rbind(dups, data.frame(axis = "x", coord = sx, points_in_strip = v[1], duplicates = v[2]))
}
for (sy in seam_y_d) {
  v <- count_dups(xmin_all, sy - DUP_W, xmax_all, sy + DUP_W)
  dups <- rbind(dups, data.frame(axis = "y", coord = sy, points_in_strip = v[1], duplicates = v[2]))
}
if (nrow(dups)) {
  write.csv(dups, file.path(out_dir, "seam_duplicates.csv"), row.names = FALSE)
  for (i in seq_len(nrow(dups)))
    cat(sprintf("[check_edge_artifacts] %s seam @ %.1f: %.0f points within %g m, %.0f duplicated\n",
                toupper(dups$axis[i]), dups$coord[i], dups$points_in_strip[i], DUP_W, dups$duplicates[i]))
  if (sum(dups$duplicates) > 0)
    cat("[check_edge_artifacts] NOTE: duplicated points found on the seams (see seam_duplicates.csv)\n")
}

# ---- Summary -----------------------------------------------------------------
if (!length(seam_x) && !length(seam_y)) {
  cat("[check_edge_artifacts] NOTE: no internal seams (single tile or fully overlapping)\n")
} else if (artifacts_found) {
  cat("[check_edge_artifacts] WARNING: potential edge artifacts detected (see seam_stats.csv)\n")
  quit(status = 1)
} else {
  cat("[check_edge_artifacts] PASS: all seams within normal density range\n")
}
cat("[check_edge_artifacts] done.\n")

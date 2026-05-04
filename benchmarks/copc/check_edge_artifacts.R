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
print(xmin, xmax, ymin, ymax)
'

get_bbox <- function(path) {
  out <- tryCatch(
    system2("python3", c("-", shQuote(path)), input = py_script,
            stdout = TRUE, stderr = FALSE),
    error = function(e) character(0))
  if (!length(out)) return(NULL)
  v <- as.numeric(strsplit(trimws(out), " ")[[1]])
  if (length(v) != 4L || anyNA(v)) return(NULL)
  list(xmin = v[1], xmax = v[2], ymin = v[3], ymax = v[4])
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

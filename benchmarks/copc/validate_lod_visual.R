#!/usr/bin/env Rscript
# LOD visual validation for a COPC file.
# Usage: Rscript validate_lod_visual.R <copc_path> <out_dir> [res_m]
#
# Renders one point-density PNG per depth level plus a composite, then prints
# a uniformity report (CV of density, coverage %).
options(error = function() { traceback(2); quit(status = 1) })
suppressPackageStartupMessages({
  library(lasR)
  library(terra)
})

args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 2L) stop("usage: validate_lod_visual.R <copc_path> <out_dir> [res_m]")
copc_path <- args[[1L]]
out_dir   <- args[[2L]]
res_m     <- if (length(args) >= 3L) as.numeric(args[[3L]]) else 10

stopifnot(file.exists(copc_path))
dir.create(out_dir, showWarnings = FALSE, recursive = TRUE)

label <- sub("\\.copc\\.laz$", "", basename(copc_path))

# ---- Discover max depth -------------------------------------------------------
candidate_csvs <- c(
  file.path(dirname(copc_path), paste0(label, "_lod.csv")),
  file.path(dirname(copc_path), "..", "results",
            paste0("lod_", label, ".csv"))
)
candidate_csvs <- normalizePath(candidate_csvs, mustWork = FALSE)

max_depth <- NULL
for (csv in candidate_csvs) {
  if (file.exists(csv)) {
    lod <- read.csv(csv)
    max_depth <- max(lod$depth, na.rm = TRUE)
    cat(sprintf("[validate_lod_visual] using LOD CSV %s: max_depth=%d\n", csv, max_depth))
    break
  }
}
if (is.null(max_depth)) {
  cat("[validate_lod_visual] no LOD CSV found — discovering max_depth by trial\n")
  prev_n <- -1L
  for (d in 0:10) {
    ans <- tryCatch(
      exec(reader(depth = d) + summarise(), on = copc_path, progress = FALSE),
      error = function(e) NULL)
    cur_n <- if (is.null(ans)) 0L else as.integer(ans$npoints)
    if (cur_n == 0L || cur_n == prev_n) { max_depth <- d - 1L; break }
    prev_n    <- cur_n
    max_depth <- d
  }
  if (is.null(max_depth) || max_depth < 0L) max_depth <- 5L
  cat(sprintf("[validate_lod_visual] discovered max_depth=%d\n", max_depth))
}

depths <- 0:max_depth

# ---- Compute density rasters per depth ----------------------------------------
rasters        <- vector("list", length(depths))
npoints_depths <- numeric(length(depths))

for (i in seq_along(depths)) {
  d <- depths[i]
  cat(sprintf("[validate_lod_visual] depth=%d: rasterizing (res=%gm)...\n", d, res_m))

  pipeline_d <- reader(depth = d) + lasR::rasterize(res_m, "count")
  r <- tryCatch(
    exec(pipeline_d, on = copc_path, progress = FALSE),
    error = function(e) {
      cat(sprintf("[validate_lod_visual] depth=%d ERROR: %s\n", d, conditionMessage(e)))
      NULL
    })

  if (!is.null(r)) {
    rasters[[i]] <- r
    npoints_depths[i] <- sum(values(r, na.rm = TRUE), na.rm = TRUE)
    cat(sprintf("[validate_lod_visual] depth=%d: %.0f points\n", d, npoints_depths[i]))
  }
}

# ---- Shared colour scale (99th pct of max-depth layer) -----------------------
all_vals   <- unlist(lapply(rasters, function(r) if (!is.null(r)) values(r, na.rm = TRUE)))
global_max <- if (length(all_vals)) quantile(all_vals, 0.99, na.rm = TRUE) else 1
pal        <- hcl.colors(256, "YlOrRd")

plot_depth <- function(r, d, n) {
  if (is.null(r)) { plot.new(); title(main = sprintf("depth=%d FAILED", d)); return() }
  plot(clamp(r, 0, global_max),
       col    = pal,
       range  = c(0, global_max),
       main   = sprintf("depth=%d | %.0f pts | res=%gm", d, n, res_m),
       legend = TRUE)
}

# ---- Per-depth PNGs -----------------------------------------------------------
for (i in seq_along(depths)) {
  d       <- depths[i]
  out_png <- file.path(out_dir, sprintf("lod_depth_%02d.png", d))
  png(out_png, width = 900, height = 820, res = 120)
  par(mar = c(4, 4, 3, 5))
  plot_depth(rasters[[i]], d, npoints_depths[i])
  dev.off()
  cat(sprintf("[validate_lod_visual] wrote %s\n", out_png))
}

# ---- Composite PNG ------------------------------------------------------------
ncols         <- min(3L, length(depths))
nrows         <- ceiling(length(depths) / ncols)
composite_png <- file.path(out_dir, "lod_composite.png")
png(composite_png, width = 900 * ncols, height = 820 * nrows, res = 120)
par(mfrow = c(nrows, ncols), mar = c(3, 3, 2.5, 4))
for (i in seq_along(depths)) plot_depth(rasters[[i]], depths[i], npoints_depths[i])
dev.off()
cat(sprintf("[validate_lod_visual] wrote composite: %s\n", composite_png))

# ---- Uniformity report --------------------------------------------------------
cat(sprintf("\n[validate_lod_visual] Uniformity report — %s (res=%gm)\n", label, res_m))
cat(sprintf("  %-8s %-14s %-14s %-10s %-10s %s\n",
            "depth", "points", "coverage_%", "med_dens", "cv_dens", "verdict"))

for (i in seq_along(depths)) {
  d <- depths[i]
  r <- rasters[[i]]
  n <- npoints_depths[i]
  if (is.null(r) || n == 0) {
    cat(sprintf("  %-8d %-14s %-14s %-10s %-10s SKIP\n", d, "n/a", "n/a", "n/a", "n/a"))
    next
  }
  vals    <- as.vector(values(r, na.rm = TRUE))
  nonzero <- vals[vals > 0]
  cov_pct <- 100 * length(nonzero) / ncell(r)
  med_d   <- if (length(nonzero)) median(nonzero) else 0
  cv_d    <- if (length(nonzero) > 1 && mean(nonzero) > 0) sd(nonzero) / mean(nonzero) else NA
  verdict <- if (is.na(cv_d)) "n/a" else if (cv_d < 0.6) "GOOD" else if (cv_d < 1.2) "MODERATE" else "UNEVEN"
  cat(sprintf("  %-8d %-14.0f %-14.1f %-10.2f %-10.3f %s\n",
              d, n, cov_pct, med_d, ifelse(is.na(cv_d), NA_real_, cv_d), verdict))
}

cat("\n[validate_lod_visual] done.\n")

#!/usr/bin/env Rscript
# Tests whether the denser cells of a COPC's coarse levels are the ones whose
# point column crosses an octree cell boundary in z.
# Usage: Rscript check_z_plane_bands.R <copc_path> [res_m] [max_depth]
#
# For each depth d, the points visible at that depth are counted on a res_m
# grid. A cell is "high" when its count exceeds HIGH_FACTOR x the median. A
# cell "crosses" when the z range of all of its points spans a boundary between
# two depth-d octree cells. The report gives the share of crossing cells among
# the high cells, among the other cells, and the median count of each group.
options(error = function() { traceback(2); quit(status = 1) })
suppressPackageStartupMessages({
  library(lasR)
  library(terra)
})

args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 1L) stop("usage: check_z_plane_bands.R <copc_path> [res_m] [max_depth]")
copc_path <- args[[1L]]
res_m     <- if (length(args) >= 2L) as.numeric(args[[2L]]) else 10
max_depth <- if (length(args) >= 3L) as.integer(args[[3L]]) else 4L
stopifnot(file.exists(copc_path))

HIGH_FACTOR <- 1.25

# The COPC info VLR is the first VLR; its payload starts 54 bytes after the
# public header and begins with center x, y, z and halfsize.
con <- file(copc_path, "rb")
invisible(seek(con, 94))
hdr_size <- readBin(con, integer(), n = 1L, size = 2L, signed = FALSE, endian = "little")
invisible(seek(con, hdr_size + 54))
info <- readBin(con, double(), n = 4L, size = 8L, endian = "little")
close(con)
halfsize <- info[4L]
z0       <- info[3L] - halfsize

label <- sub("\\.copc\\.laz$", "", basename(copc_path))
full  <- exec(reader() + lasR::rasterize(res_m, c("count", "z_min", "z_max")),
              on = copc_path, progress = FALSE)
zmin  <- as.vector(values(full[["z_min"]]))
zmax  <- as.vector(values(full[["z_max"]]))

cat(sprintf("[check_z_plane_bands] %s: octree z0=%.2f halfsize=%.2f, data z %.1f..%.1f, res=%gm\n",
            label, z0, halfsize, min(zmin, na.rm = TRUE), max(zmax, na.rm = TRUE), res_m))
cat(sprintf("  %-6s %-10s %-12s %-16s %-16s %-14s %s\n",
            "depth", "cell_z_m", "crossing_%", "crossing_%_high", "crossing_%_rest",
            "med_crossing", "med_not_crossing"))

for (d in 0:max_depth) {
  cell <- 2 * halfsize / 2^d
  lod  <- exec(reader(depth = d) + lasR::rasterize(res_m, "count"),
               on = copc_path, progress = FALSE)
  cnt  <- as.vector(values(lod))
  ok   <- !is.na(cnt) & cnt > 0 & !is.na(zmin)
  crosses <- floor((zmin - z0) / cell) != floor((zmax - z0) / cell)
  high    <- cnt > HIGH_FACTOR * median(cnt[ok])
  pct <- function(sel) if (any(sel)) 100 * mean(crosses[sel]) else NA_real_
  med <- function(sel) if (any(sel)) median(cnt[sel]) else NA_real_
  cat(sprintf("  %-6d %-10.2f %-12.1f %-16.1f %-16.1f %-14.0f %.0f\n",
              d, cell, pct(ok), pct(ok & high), pct(ok & !high),
              med(ok & crosses), med(ok & !crosses)))
}
cat("[check_z_plane_bands] done.\n")

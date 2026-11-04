#!/usr/bin/env Rscript
# Merges multiple LAZ tiles into a single COPC using lasR experimental writer.
# Usage: Rscript write_lasR_multitile.R <tiles_dir> <output_copc> [density]
#   density: "normal" (default) | "dense"
options(error = function() { traceback(2); quit(status = 1) })
suppressPackageStartupMessages(library(lasR))

args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 2L) stop("usage: write_lasR_multitile.R <tiles_dir> <output_copc> [density]")

tiles_dir <- args[[1L]]
output    <- args[[2L]]
density   <- if (length(args) >= 3L) args[[3L]] else "normal"
stopifnot(density %in% c("normal", "dense"))

inputs <- list.files(tiles_dir, pattern = "(?i)\\.la[sz]$",
                     full.names = TRUE, recursive = FALSE)
if (!length(inputs)) stop(sprintf("no LAZ/LAS files in: %s", tiles_dir))

cat(sprintf("[write_lasR_multitile] merging %d tiles (density=%s):\n",
            length(inputs), density))
for (f in inputs) cat(sprintf("  %s (%.1f MB)\n", basename(f), file.size(f) / 1e6))
cat(sprintf("  → %s\n", output))

dir.create(dirname(output), showWarnings = FALSE, recursive = TRUE)

pipeline <- reader() + write_copc(
  ofile               = output,
  density             = density,
  experimental_writer = TRUE
)

t0 <- proc.time()
exec(pipeline, on = inputs, progress = FALSE)
elapsed <- (proc.time() - t0)[["elapsed"]]

if (!file.exists(output) || file.size(output) == 0L)
  stop(sprintf("output not produced or empty: %s", output))

cat(sprintf("[write_lasR_multitile] wrote %.1f MB in %.1f s\n",
            file.size(output) / 1e6, elapsed))

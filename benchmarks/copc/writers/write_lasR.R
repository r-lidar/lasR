#!/usr/bin/env Rscript
# Usage:
#   write_lasR.R <variant> <input_laz> <output_copc_laz>
# variant: "default" | "experimental" | "experimental-dense"
suppressPackageStartupMessages(library(lasR))

args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 3L) stop("usage: write_lasR.R <variant> <input> <output>")
variant <- args[[1L]]
input   <- args[[2L]]
output  <- args[[3L]]

stopifnot(file.exists(input))
stopifnot(variant %in% c("default", "experimental", "experimental-dense"))

experimental <- variant %in% c("experimental", "experimental-dense")
density <- if (identical(variant, "experimental-dense")) "dense" else "normal"

pipeline <- reader() + write_copc(
  ofile = output,
  density = density,
  experimental_writer = experimental
)
ans <- exec(pipeline, on = input, progress = FALSE)

if (!file.exists(output) || file.size(output) == 0L) {
  stop(sprintf("write_lasR.R: output not produced or empty: %s", output))
}
cat(sprintf("[write_lasR variant=%s density=%s] wrote %s (%d bytes)\n",
            variant, density, output, file.size(output)))

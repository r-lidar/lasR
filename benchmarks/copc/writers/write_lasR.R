#!/usr/bin/env Rscript
# Usage:
#   write_lasR.R <variant> <input_laz> <output_copc_laz>
# variant: "default" | "experimental"
suppressPackageStartupMessages(library(lasR))

args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 3L) stop("usage: write_lasR.R <variant> <input> <output>")
variant <- args[[1L]]
input   <- args[[2L]]
output  <- args[[3L]]

stopifnot(file.exists(input))
stopifnot(variant %in% c("default", "experimental"))

experimental <- identical(variant, "experimental")

pipeline <- reader() + write_copc(ofile = output, experimental_writer = experimental)
ans <- exec(pipeline, on = input, progress = FALSE)

if (!file.exists(output) || file.size(output) == 0L) {
  stop(sprintf("write_lasR.R: output not produced or empty: %s", output))
}
cat(sprintf("[write_lasR variant=%s] wrote %s (%d bytes)\n",
            variant, output, file.size(output)))

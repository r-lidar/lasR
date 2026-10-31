#!/usr/bin/env Rscript
# Regenerates results/REPORT.md from versions.txt, run_metadata.txt,
# write.csv, lod_<writer>.csv, and read.csv.
options(error = function() { traceback(2); quit(status = 1) })

stopifnot(dir.exists("results"))
res <- "results"
out <- file.path(res, "REPORT.md")
stopifnot(file.exists(file.path(res, "write.csv")))

write_df <- read.csv(file.path(res, "write.csv"))
read_df <- if (file.exists(file.path(res, "read.csv"))) read.csv(file.path(res, "read.csv")) else NULL
meta <- if (file.exists(file.path(res, "run_metadata.txt"))) readLines(file.path(res, "run_metadata.txt")) else character()
versions <- if (file.exists(file.path(res, "versions.txt"))) readLines(file.path(res, "versions.txt")) else character()

fmt_n <- function(x, digits = 2) formatC(x, format = "f", digits = digits, big.mark = ",")
quantile_safe <- function(x, p) {
  x <- x[!is.na(x)]
  if (!length(x)) return(NA_real_)
  as.numeric(quantile(x, p, na.rm = TRUE))
}

lines <- c(
  "# COPC writer benchmark report",
  "",
  paste0("Generated: ", format(Sys.time(), "%Y-%m-%dT%H:%M:%S")),
  "",
  "## Run metadata",
  "",
  "```",
  meta,
  "```",
  "",
  "## Tool versions",
  "",
  "```",
  versions,
  "```",
  ""
)

warm <- write_df[write_df$run_kind == "warm" & write_df$exit_status == 0, ]
cold <- write_df[write_df$run_kind == "cold" & write_df$exit_status == 0, ]
writer_order <- sort(unique(warm$writer_id))
write_summary <- if (length(writer_order)) do.call(rbind, lapply(writer_order, function(w) {
  d <- warm[warm$writer_id == w, , drop = FALSE]
  cold_w <- cold$wall_s[cold$writer_id == w]
  data.frame(
    writer_id = w,
    wall_median = median(d$wall_s, na.rm = TRUE),
    wall_q1 = quantile_safe(d$wall_s, .25),
    wall_q3 = quantile_safe(d$wall_s, .75),
    cold_wall_s = if (length(cold_w)) cold_w[[1L]] else NA_real_,
    peak_rss_median = median(d$peak_rss_mb, na.rm = TRUE),
    output_mb = median(d$output_bytes, na.rm = TRUE) / 1024 / 1024
  )
})) else data.frame()

lines <- c(lines,
  "## Write phase (warm runs, all writers)",
  "",
  "| writer | wall_s (median) | wall_s (IQR) | peak RSS MB (median) | output MB |",
  "|---|---:|---:|---:|---:|"
)
for (i in seq_len(nrow(write_summary))) {
  cold_str <- if (!is.na(write_summary$cold_wall_s[i]))
    sprintf(" (cold: %.2fs)", write_summary$cold_wall_s[i]) else ""
  lines <- c(lines, sprintf("| %s | %.2f%s | %.2f-%.2f | %.1f | %.1f |",
    as.character(write_summary$writer_id[i]), write_summary$wall_median[i], cold_str,
    write_summary$wall_q1[i], write_summary$wall_q3[i],
    write_summary$peak_rss_median[i], write_summary$output_mb[i]))
}
lines <- c(lines, "")

lod_files <- list.files(res, pattern = "^lod_.*\\.csv$", full.names = TRUE)
lod_files <- lod_files[basename(lod_files) != "lod_summary.csv"]
if (length(lod_files)) {
  lines <- c(lines, "## LOD organisation", "",
             "| writer | leaves | max_depth | nodes@d0 | nodes@d1 | nodes@d2 | nodes@d3 | total_points |",
             "|---|---:|---:|---:|---:|---:|---:|---:|")
  for (f in lod_files) {
    d <- read.csv(f)
    if (!nrow(d)) next
    nd <- function(k) sum(d$depth == k)
    lines <- c(lines, sprintf("| %s | %d | %d | %d | %d | %d | %d | %s |",
      d$writer_id[[1L]], nrow(d), max(d$depth), nd(0), nd(1), nd(2), nd(3),
      fmt_n(sum(d$point_count), 0)))
  }
  lines <- c(lines, "")
}

if (!is.null(read_df)) {
  for (sc in unique(read_df$scenario)) {
    sub <- read_df[read_df$scenario == sc & read_df$hosting == "m2", ]
    if (!nrow(sub)) next
    lines <- c(lines,
      sprintf("## Read phase - %s (loopback httpuv)", sc), "",
      "| writer | wall_s | requests | bytes_transferred | points | bytes/point |",
      "|---|---:|---:|---:|---:|---:|")
    for (i in seq_len(nrow(sub))) {
      bpp <- if (sub$n_points[i] > 0) sub$bytes_per_point[i] else NA_real_
      lines <- c(lines, sprintf("| %s | %.3f | %d | %s | %s | %.2f |",
        sub$writer_id[i], sub$wall_s[i], sub$n_requests[i],
        fmt_n(sub$bytes_transferred[i], 0),
        fmt_n(sub$n_points[i], 0), bpp))
    }
    lines <- c(lines, "")
  }
}

lines <- c(lines, "## Notes", "",
           "_(Manually written narrative interpreting the numbers goes here.)_", "")

writeLines(lines, out)
cat(sprintf("[make_report] wrote %s (%d lines)\n", out, length(lines)))

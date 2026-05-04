#!/usr/bin/env Rscript
# Runs R1/R2/R3 read scenarios against every promoted COPC over the local
# range-logging server and writes results/read.csv.
options(error = function() { traceback(2); quit(status = 1) })
suppressPackageStartupMessages({
  library(lasR)
  library(jsonlite)
})

args <- commandArgs(trailingOnly = TRUE)
host_label <- if (length(args) >= 1L) args[[1L]] else "m2"
log_csv <- if (length(args) >= 2L) args[[2L]] else "results/http_log.csv"
run_id_file <- if (length(args) >= 3L) args[[3L]] else "results/.run_id"
read_csv <- if (length(args) >= 4L) args[[4L]] else "results/read.csv"
base_url <- if (length(args) >= 5L) args[[5L]] else Sys.getenv("BENCH_BASE_URL", unset = "http://127.0.0.1:38080")
meta_file <- "results/run_metadata.txt"

stopifnot(file.exists(meta_file))

read_meta <- function(file) {
  raw <- readLines(file)
  raw <- raw[nzchar(trimws(raw)) & !startsWith(raw, "#")]
  m <- regmatches(raw, regexec("^(\\S+)\\s+(.*)$", raw))
  vals <- vapply(m, function(x) if (length(x) == 3L) x[[3L]] else NA_character_, character(1L))
  names(vals) <- vapply(m, function(x) if (length(x) == 3L) x[[2L]] else NA_character_, character(1L))
  vals[!is.na(names(vals))]
}

meta <- read_meta(meta_file)
xmin <- as.numeric(meta[["dataset_xmin"]])
xmax <- as.numeric(meta[["dataset_xmax"]])
ymin <- as.numeric(meta[["dataset_ymin"]])
ymax <- as.numeric(meta[["dataset_ymax"]])

select_r3_query <- function() {
  cache <- "results/r3_query.csv"
  if (file.exists(cache)) {
    q <- read.csv(cache)
    return(q[1L, c("cx", "cy", "r", "expected_npoints")])
  }

  input <- "data/input.laz"
  stopifnot(file.exists(input))

  r <- 0.05 * min(xmax - xmin, ymax - ymin)
  grid <- expand.grid(px = c(0.30, 0.50, 0.70), py = c(0.30, 0.50, 0.70))
  grid$cx <- xmin + grid$px * (xmax - xmin)
  grid$cy <- ymin + grid$py * (ymax - ymin)

  counts <- vapply(seq_len(nrow(grid)), function(i) {
    ans <- tryCatch(
      exec(reader_circles(grid$cx[i], grid$cy[i], r) + summarise(),
           on = input, progress = FALSE),
      error = function(e) NULL)
    if (is.list(ans) && !is.null(ans$npoints)) as.numeric(ans$npoints) else 0
  }, numeric(1L))

  positive <- which(counts > 0)
  if (!length(positive)) stop("R3 calibration found no non-empty candidate")
  ordered <- positive[order(counts[positive])]
  pick <- ordered[ceiling(length(ordered) / 2)]

  q <- data.frame(cx = grid$cx[pick], cy = grid$cy[pick], r = r,
                  expected_npoints = counts[pick])
  write.csv(q, cache, row.names = FALSE)
  q
}

r3_query <- select_r3_query()
cx <- r3_query$cx
cy <- r3_query$cy
r <- r3_query$r
cat(sprintf("[bench_read] R3 query cx=%.3f cy=%.3f r=%.3f expected_local_points=%d\n",
            cx, cy, r, as.integer(r3_query$expected_npoints)))

scenarios <- list(
  list(id = "R1", pipeline = function() reader() + summarise()),
  list(id = "R2-d0", pipeline = function() reader(depth = 0L) + summarise()),
  list(id = "R2-d1", pipeline = function() reader(depth = 1L) + summarise()),
  list(id = "R2-d2", pipeline = function() reader(depth = 2L) + summarise()),
  list(id = "R3", pipeline = function() reader_circles(cx, cy, r) + summarise())
)

copcs <- list.files("results/outputs", pattern = "\\.copc\\.laz$", full.names = TRUE)
if (!length(copcs)) stop("bench_read.R: no COPCs under results/outputs/")
writer_ids <- sub("\\.copc\\.laz$", "", basename(copcs))

dir.create(dirname(read_csv), showWarnings = FALSE, recursive = TRUE)
if (!file.exists(read_csv)) {
  writeLines(paste(c(
    "run_id", "writer_id", "scenario", "hosting", "wall_s",
    "n_requests", "bytes_transferred", "n_points",
    "bytes_per_point", "requests_per_million_points", "host", "ts"
  ), collapse = ","), read_csv)
}

write_run_id <- function(rid) writeLines(rid, run_id_file)

bench_one <- function(writer_id, scenario) {
  rid <- sprintf("%s__%s__%s__%s", host_label, writer_id, scenario$id,
                 format(Sys.time(), "%Y%m%d%H%M%OS3"))
  write_run_id(rid)
  url <- sprintf("%s/%s.copc.laz?bench_run=%s",
                 base_url, writer_id, utils::URLencode(rid, reserved = TRUE))

  t0 <- Sys.time()
  ans <- exec(scenario$pipeline(), on = url, progress = FALSE)
  wall_s <- as.numeric(difftime(Sys.time(), t0, units = "secs"))

  npts <- if (is.list(ans) && !is.null(ans$npoints)) ans$npoints else 0L
  list(rid = rid, wall_s = wall_s, npoints = npts)
}

log_summary <- function(rid) {
  if (!file.exists(log_csv)) return(c(n = 0L, bytes = 0))
  log <- read.csv(log_csv, stringsAsFactors = FALSE)
  hit <- log$run_id == rid
  c(n = sum(hit), bytes = sum(as.numeric(log$response_bytes[hit]), na.rm = TRUE))
}

failures <- character()
require_http_metrics <- identical(host_label, "m2")

for (writer_id in writer_ids) {
  for (sc in scenarios) {
    cat(sprintf("[bench_read] %s / %s\n", writer_id, sc$id))
    out <- tryCatch(bench_one(writer_id, sc),
                    error = function(e) {
                      cat(sprintf("[bench_read] FAIL %s/%s: %s\n",
                                  writer_id, sc$id, conditionMessage(e)))
                      failures <<- c(failures, sprintf("%s/%s: %s",
                                                       writer_id, sc$id,
                                                       conditionMessage(e)))
                      list(rid = NA_character_, wall_s = NA_real_, npoints = 0L)
                    })
    Sys.sleep(0.2)
    summ <- if (is.na(out$rid)) c(n = 0L, bytes = 0) else log_summary(out$rid)
    if (require_http_metrics && !is.na(out$rid) &&
        (summ[["n"]] <= 0L || summ[["bytes"]] <= 0)) {
      failures <- c(failures, sprintf("%s/%s: zero HTTP metrics (requests=%s, bytes=%s)",
                                      writer_id, sc$id, summ[["n"]], summ[["bytes"]]))
    }
    if (identical(sc$id, "R3") && out$npoints <= 0) {
      failures <- c(failures, sprintf("%s/R3: zero returned points", writer_id))
    }
    bpp <- if (out$npoints > 0) summ[["bytes"]] / out$npoints else NA_real_
    rpm <- if (out$npoints > 0) summ[["n"]] * 1e6 / out$npoints else NA_real_
    line <- paste(c(
      out$rid, writer_id, sc$id, host_label,
      format(out$wall_s, nsmall = 4),
      summ[["n"]], summ[["bytes"]], out$npoints,
      format(bpp, nsmall = 4), format(rpm, nsmall = 4),
      Sys.info()[["nodename"]],
      format(Sys.time(), "%Y-%m-%dT%H:%M:%OS3")
    ), collapse = ",")
    cat(line, "\n", sep = "", file = read_csv, append = TRUE)
  }
}

if (length(failures)) {
  stop(sprintf("read benchmark failures:\n- %s", paste(failures, collapse = "\n- ")))
}
cat(sprintf("[bench_read] done. read.csv rows: %d\n", nrow(read.csv(read_csv))))

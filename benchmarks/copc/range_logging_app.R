#!/usr/bin/env Rscript
# Usage:
#   range_logging_app.R <serve_dir> <port> <log_csv> [run_id_file]
#
# Serves files under <serve_dir>, supports GET/HEAD byte ranges, and logs
# every request to <log_csv>. The current run_id is read on each request.
options(error = function() { traceback(2); quit(status = 1) })
suppressPackageStartupMessages(library(httpuv))

args <- commandArgs(trailingOnly = TRUE)
if (length(args) < 3L) stop("usage: range_logging_app.R <dir> <port> <log_csv> [run_id_file]")
serve_dir <- normalizePath(args[[1L]], mustWork = TRUE)
port <- as.integer(args[[2L]])
log_csv <- args[[3L]]
run_id_file <- if (length(args) >= 4L) args[[4L]] else ""

dir.create(dirname(log_csv), showWarnings = FALSE, recursive = TRUE)
if (!file.exists(log_csv)) {
  writeLines(
    "ts,run_id,method,path,status,range_header,range_start,range_end,content_length,response_bytes,response_time_s",
    log_csv)
}

current_run_id <- function() {
  if (!nzchar(run_id_file) || !file.exists(run_id_file)) return("unknown")
  rid <- tryCatch(trimws(readLines(run_id_file, n = 1L, warn = FALSE)),
                  error = function(e) "unknown")
  if (length(rid) == 0L || !nzchar(rid)) "unknown" else rid
}

csv_field <- function(x) {
  s <- as.character(if (is.null(x)) "" else x)
  if (grepl('[,"\n]', s)) paste0('"', gsub('"', '""', s), '"') else s
}

log_row <- function(...) {
  fields <- vapply(list(...), csv_field, character(1L))
  cat(paste(fields, collapse = ","), "\n", sep = "", file = log_csv, append = TRUE)
}

resolve_path <- function(req_path) {
  p <- sub("\\?.*$", "", req_path)
  p <- utils::URLdecode(p)
  full <- normalizePath(file.path(serve_dir, sub("^/+", "", p)),
                        winslash = "/", mustWork = FALSE)
  if (!(full == serve_dir || startsWith(full, paste0(serve_dir, "/")))) return(NULL)
  full
}

parse_range <- function(hdr, file_size) {
  if (is.null(hdr) || !nzchar(hdr)) return(NULL)
  m <- regmatches(hdr, regexec("^bytes=([0-9]*)-([0-9]*)$", hdr))[[1L]]
  if (length(m) != 3L) return(list(ok = FALSE))
  s_str <- m[[2L]]
  e_str <- m[[3L]]
  if (!nzchar(s_str) && !nzchar(e_str)) return(list(ok = FALSE))

  if (!nzchar(s_str)) {
    n <- as.numeric(e_str)
    start <- max(0, file_size - n)
    end <- file_size - 1
  } else if (!nzchar(e_str)) {
    start <- as.numeric(s_str)
    end <- file_size - 1
  } else {
    start <- as.numeric(s_str)
    end <- as.numeric(e_str)
  }

  if (start > end || start < 0 || end >= file_size) return(list(ok = FALSE))
  list(ok = TRUE, start = start, end = end)
}

read_range <- function(path, start, end) {
  con <- file(path, "rb")
  on.exit(close(con), add = TRUE)
  seek(con, where = start, origin = "start")
  readBin(con, raw(), n = end - start + 1L)
}

app <- list(
  call = function(req) {
    t0 <- Sys.time()
    method <- req$REQUEST_METHOD
    path_raw <- req$PATH_INFO
    range_hdr <- req$HTTP_RANGE
    run_id <- current_run_id()

    if (!(method %in% c("GET", "HEAD"))) {
      msg <- charToRaw("method not allowed")
      body <- if (identical(method, "HEAD")) raw() else msg
      log_row(format(t0, "%Y-%m-%dT%H:%M:%OS3"), run_id, method, path_raw,
              405L, range_hdr, NA, NA, length(msg), length(body),
              as.numeric(Sys.time() - t0))
      return(list(status = 405L,
                  headers = list("Content-Type" = "text/plain",
                                 "Content-Length" = as.character(length(msg)),
                                 "Allow" = "GET, HEAD"),
                  body = body))
    }

    path <- resolve_path(path_raw)
    if (is.null(path) || !file.exists(path) || dir.exists(path)) {
      msg <- charToRaw("not found")
      body <- if (identical(method, "HEAD")) raw() else msg
      log_row(format(t0, "%Y-%m-%dT%H:%M:%OS3"), run_id, method, path_raw,
              404L, range_hdr, NA, NA, length(msg), length(body),
              as.numeric(Sys.time() - t0))
      return(list(status = 404L,
                  headers = list("Content-Type" = "text/plain",
                                 "Content-Length" = as.character(length(msg))),
                  body = body))
    }

    fsize <- file.info(path)$size
    rng <- parse_range(range_hdr, fsize)

    if (is.null(rng)) {
      body <- if (identical(method, "HEAD")) raw() else readBin(path, raw(), n = fsize)
      headers <- list("Content-Type" = "application/octet-stream",
                      "Content-Length" = as.character(fsize),
                      "Accept-Ranges" = "bytes")
      log_row(format(t0, "%Y-%m-%dT%H:%M:%OS3"), run_id, method, path_raw,
              200L, NA, NA, NA, fsize, length(body),
              as.numeric(Sys.time() - t0))
      return(list(status = 200L, headers = headers, body = body))
    }

    if (isFALSE(rng$ok)) {
      headers <- list("Content-Range" = paste0("bytes */", fsize))
      log_row(format(t0, "%Y-%m-%dT%H:%M:%OS3"), run_id, method, path_raw,
              416L, range_hdr, NA, NA, 0L, 0L,
              as.numeric(Sys.time() - t0))
      return(list(status = 416L, headers = headers, body = raw()))
    }

    content_length <- rng$end - rng$start + 1
    body <- if (identical(method, "HEAD")) raw() else read_range(path, rng$start, rng$end)
    headers <- list("Content-Type" = "application/octet-stream",
                    "Content-Length" = as.character(content_length),
                    "Content-Range" = sprintf("bytes %.0f-%.0f/%.0f",
                                              rng$start, rng$end, fsize),
                    "Accept-Ranges" = "bytes")
    log_row(format(t0, "%Y-%m-%dT%H:%M:%OS3"), run_id, method, path_raw,
            206L, range_hdr, rng$start, rng$end, content_length, length(body),
            as.numeric(Sys.time() - t0))
    list(status = 206L, headers = headers, body = body)
  }
)

server <- startServer("127.0.0.1", port, app)
cat(sprintf("[range_logging_app] listening on http://127.0.0.1:%d serving %s\n",
            port, serve_dir))

on.exit(stopServer(server), add = TRUE)
while (TRUE) {
  service(50)
  Sys.sleep(0.001)
}

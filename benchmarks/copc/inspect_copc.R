#!/usr/bin/env Rscript
# Usage: inspect_copc.R <writer_id> <copc_path> <out_csv>
# Walks the COPC info VLR and hierarchy eVLR. Writes one row per node with
# columns: writer_id, depth, x, y, z, point_count, byte_offset, byte_size.
options(error = function() { traceback(2); quit(status = 1) })

args <- commandArgs(trailingOnly = TRUE)
if (length(args) != 3L) stop("usage: inspect_copc.R <writer_id> <path> <out_csv>")
writer_id <- args[[1L]]; path <- args[[2L]]; out_csv <- args[[3L]]
stopifnot(file.exists(path))

con <- file(path, "rb"); on.exit(close(con), add = TRUE)

read_u8  <- function() readBin(con, integer(), n = 1L, size = 1L, signed = FALSE, endian = "little")
read_u16 <- function() readBin(con, integer(), n = 1L, size = 2L, signed = FALSE, endian = "little")
# R's readBin() doesn't support signed = FALSE for size >= 4, so build u32/u64 from raw bytes.
# Result is a numeric (double) so values up to 2^53 fit exactly — fine for COPC offsets/sizes.
read_u32 <- function() {
  bytes <- as.integer(readBin(con, raw(), n = 4L))
  bytes[1L] + bytes[2L] * 256 + bytes[3L] * 65536 + bytes[4L] * 16777216
}
read_u64 <- function() {
  bytes <- as.integer(readBin(con, raw(), n = 8L))
  lo <- bytes[1L] + bytes[2L] * 256 + bytes[3L] * 65536 + bytes[4L] * 16777216
  hi <- bytes[5L] + bytes[6L] * 256 + bytes[7L] * 65536 + bytes[8L] * 16777216
  hi * 2^32 + lo
}
read_i32 <- function() readBin(con, integer(), n = 1L, size = 4L, signed = TRUE, endian = "little")
read_f64 <- function() readBin(con, double(), n = 1L, size = 8L, endian = "little")
read_str <- function(n) rawToChar(readBin(con, raw(), n = n))
# seek() returns the previous position; wrap in invisible() to keep stdout clean.
seek_to  <- function(pos) invisible(seek(con, where = pos, origin = "start"))

# Public LAS 1.4 header
seek_to(0)
sig <- read_str(4); stopifnot(sig == "LASF")
seek_to(94);  hdr_size      <- read_u16()
seek_to(96);  offset_to_pts <- read_u32()
              n_vlr         <- read_u32()
              pt_fmt_byte   <- read_u8()
              pt_record_len <- read_u16()
seek_to(235); start_first_evlr <- read_u64()
seek_to(243); n_evlr        <- read_u32()

# COPC info VLR is the first VLR. Walk VLRs to find user_id="copc", record_id=1.
seek_to(hdr_size)
copc_info <- NULL
for (i in seq_len(n_vlr)) {
  reserved  <- read_u16()
  user_id   <- gsub("\\0+$", "", read_str(16))
  record_id <- read_u16()
  rec_len   <- read_u16()
  desc      <- read_str(32)
  rec_start <- seek(con)
  if (user_id == "copc" && record_id == 1L) {
    center_x <- read_f64(); center_y <- read_f64(); center_z <- read_f64()
    halfsize <- read_f64()
    spacing  <- read_f64()
    root_h_offset <- read_u64()
    root_h_size   <- read_u64()
    gpstime_min   <- read_f64(); gpstime_max <- read_f64()
    copc_info <- list(center = c(center_x, center_y, center_z),
                      halfsize = halfsize, spacing = spacing,
                      root_h_offset = root_h_offset, root_h_size = root_h_size)
  }
  seek_to(rec_start + rec_len)
}
if (is.null(copc_info)) stop("inspect_copc.R: COPC info VLR not found")

# Walk hierarchy pages starting from root. Each page is `root_h_size` bytes
# at `root_h_offset`, packed as
#   <i32 d, i32 x, i32 y, i32 z, u64 offset, i32 byte_size, i32 point_count>
# (32 bytes per entry). Child pages have point_count=-1, byte_size points to a sub-page.
read_page <- function(offset, size) {
  seek_to(offset)
  n <- size %/% 32L
  rows <- vector("list", n)
  for (k in seq_len(n)) {
    d  <- read_i32(); x <- read_i32(); y <- read_i32(); z <- read_i32()
    bo <- read_u64()
    bs <- read_i32()
    pc <- read_i32()
    rows[[k]] <- data.frame(d = d, x = x, y = y, z = z,
                            byte_offset = bo, byte_size = bs, point_count = pc)
  }
  do.call(rbind, rows)
}

walk <- function(off, sz, acc) {
  page <- read_page(off, sz)
  leaves <- page[page$point_count >= 0L, , drop = FALSE]
  if (nrow(leaves)) acc[[length(acc) + 1L]] <- leaves
  subpages <- page[page$point_count < 0L, , drop = FALSE]
  for (i in seq_len(nrow(subpages))) {
    acc <- walk(subpages$byte_offset[i], subpages$byte_size[i], acc)
  }
  acc
}

leaves_list <- walk(copc_info$root_h_offset, copc_info$root_h_size, list())
leaves <- if (length(leaves_list)) do.call(rbind, leaves_list) else
          data.frame(d = integer(), x = integer(), y = integer(), z = integer(),
                     byte_offset = numeric(), byte_size = integer(), point_count = integer())

leaves$writer_id <- writer_id
leaves <- leaves[, c("writer_id", "d", "x", "y", "z", "point_count", "byte_offset", "byte_size")]
names(leaves) <- c("writer_id", "depth", "x", "y", "z", "point_count", "byte_offset", "byte_size")

dir.create(dirname(out_csv), showWarnings = FALSE, recursive = TRUE)
write.csv(leaves, out_csv, row.names = FALSE)

cat(sprintf("[inspect_copc] %s: %d leaves, max_depth=%d, total_points=%.0f\n",
            writer_id, nrow(leaves),
            if (nrow(leaves)) max(leaves$depth) else 0L,
            sum(leaves$point_count)))

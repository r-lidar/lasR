#!/usr/bin/env Rscript

ti <- Sys.time()

args <- commandArgs(trailingOnly = TRUE)
args <- as.list(args)

print_help <- function() {
  cat("Usage: lasr command -i input [args] [options]
Example: lasr chm -i /path/to/folder/ -o /path/to/chm.tif -res 1 -ncores 4
Possible commands and arguments:
  help
  info
  chm
    -res (defaul 1)
    -o /output/file.tif
  dtm
    -res (defaul 1)
    -o /output/file.tif
  denoise
    -res (defaul 5)
    -n (default 6)
    -o /output/*.laz
General available options
   -ncores 4
   -noprogress
  ")
  q("no")
}

get_param <- function(args, name, default = NULL) {
  index <- which(sapply(args, function(item) item == name))
  if (length(index) == 0) {
    return(default)
  } else {
    return(args[[index + 1]])
  }
}

is_set_flag <- function(args, name) {
  index <- which(sapply(args, function(item) item == name))
  if (length(index) == 0) {
    return(FALSE)
  } else {
    return(TRUE)
  }
}

available_commands <- c("help", "info", "chm")

input <- get_param(args, "-i", NA_character_)
output <- get_param(args, "-o", NA_character_)
ncores <- get_param(args, "-ncores", lasR::concurrent_points())
progress <- !is_set_flag(args, "-noprogress")

if (length(args) < 2) print_help()

cmd <- try(match.arg(args[[1]], available_commands), silent = TRUE)

if (is(cmd, "try-error")) print_help()

if (cmd == "help") {
  print_help()
} else if (cmd == "info") {
  progress <- FALSE
  pipeline <- lasR::info()
} else if (cmd == "chm") {
  res <- as.numeric(get_param(args, "-res", 1))
  pipeline <- lasR::rasterize(res, "max", ofile = output)
} else if (cmd == "dtm") {
  res <- as.numeric(get_param(args, "-res", 1))
  tri <- lasR::triangulate(filter = lasR::keep_ground_and_water())
  dtm <- lasR::rasterize(res, tin)
  pipeline <- tri + dtm
} else if (cmd == "denoise") {
  res <- get_param(args, "res", 5)
  n <- get_param(args, "n", 6)
  noise <- lasR::classify_with_ivf(res, n)
  write <- lasR::write_las(output, filter = lasR::drop_noise())
  pipeline <- noise + write
}

ans <- lasR::exec(pipeline, on = input, progress = progress, ncores = ncores, noread = TRUE)
if (!is.null(ans)) print(ans)

tf <- Sys.time()
difftime(tf, ti)

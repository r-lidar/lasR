#!/usr/bin/env Rscript

ti <- Sys.time()

args <- commandArgs(trailingOnly = TRUE)
args <- as.list(args)

print_help <- function() {
  cat("Usage: lasr command -i input [args] [options]
Example: lasr chm -i /path/to/folder/ -o /path/to/chm.tif -res 1 -ncores 4
Possible commands and arguments:
  help
  info: display info about the file
  lax: write lax files from a collection of la[s|z] files
    -embedded
    -overwrite
  vpc: write vpc file from a collection of la[s|z] files
    -absolute
    -gps
  chm: compute a canopy height model
    -res 0.5 (default 1)
    -o /output/file.tif
  dtm: compute a digital terrain model
    -res 0.5 (default 1)
    -o /output/file.tif
  denoise: remove noise points
    -res 4 (default 5)
    -n 5 (default 6)
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

available_commands <- c("help", "info", "lax", "vpc", "chm", "dtm", "denoise", "sampling_tls")

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
} else if (cmd == "lax") {
  embedded <- is_set_flag(args, "-embedded")
  overwrite <- is_set_flag(args, "-overwrite")
  pipeline = lasR::write_lax(embedded, overwrite)
} else if (cmd == "vpc") {
  progress <- FALSE
  absolute <- is_set_flag(args, "-absolute")
  gps <- is_set_flag(args, "-gps")
  pipeline <- lasR::write_vpc()
} else if (cmd == "chm") {
  res <- as.numeric(get_param(args, "-res", 1))
  pipeline <- lasR::rasterize(res, "max", ofile = output)
} else if (cmd == "dtm") {
  res <- as.numeric(get_param(args, "-res", 1))
  tri <- lasR::triangulate(filter = lasR::keep_ground_and_water())
  dtm <- lasR::rasterize(res, tin)
  pipeline <- tri + dtm
} else if (cmd == "denoise") {
  res <- as.numeric(get_param(args, "-res", 5))
  n <- as.numeric(get_param(args, "n", 6))
  noise <- lasR::classify_with_ivf(res, n)
  write <- lasR::write_las(output, filter = lasR::drop_noise())
  pipeline <- noise + write
} else if (cmd == "sampling_tls") {
  res <- as.numeric(get_param(args, "-res", 1))
  sample <- lasR::sampling_voxel(res)
  sample$sampling_voxel$shuffle_size = 0
  write <- lasR::write_las(output)
  pipeline <- sample + write
}


ans <- lasR::exec(pipeline, on = input, progress = progress, ncores = ncores, noread = TRUE)
if (!is.null(ans)) print(ans)

tf <- Sys.time()
difftime(tf, ti)

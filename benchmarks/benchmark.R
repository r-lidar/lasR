#!/usr/bin/env Rscript
args = commandArgs(trailingOnly=TRUE)

lasR = TRUE
test = 0
multifile = FALSE

if (length(args) < 2) {
  stop("At least 2 argument must be supplied", call.=FALSE)
} else {
  lasR = args[1] == "lasR"
  test = as.integer(args[2])
  multifile = !is.null(args[3]) && args[3] == "true"
}

cat("lasR:", lasR, "\n")
cat("test:", test, "\n")
cat("multifile:", multifile, "\n")

library(lidR)
library(lasR)

if (!multifile)
{
  if (lasR)
    set_parallel_strategy(concurrent_points(half_cores()))
  else
    set_lidr_threads(half_cores())
} else {
  if (lasR)
    set_parallel_strategy(concurrent_files(4))
  else
    future::plan(future::multicore(workers = 4))
}

f = c("/home/jr/Documents/Ulaval/ALS data/BCTS//092L072244_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS//092L072422_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS//092L073133_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS//092L073311_BCTS_2.laz")

#f = system.file("extdata", "bcts/", package = "lasR")

ti = Sys.time()

if (test == 1)
{
  if (lasR)
  {
    pipeline = rasterize(1, "max")
    exec(pipeline, on = f, progress = TRUE, noread = TRUE)
  }
  else
  {
    ctg = readLAScatalog(f)
    chm = rasterize_canopy(ctg, 1, p2r())
  }
}

if (test == 2)
{
  if (lasR)
  {
    tri = triangulate()
    pipeline = reader_las(filter = keep_ground()) + tri + rasterize(1, tri)
    exec(pipeline, f, progress = TRUE, noread = TRUE)
  }
  else
  {
    ctg = readLAScatalog(f)
    dtm = rasterize_terrain(ctg, 1, tin())
  }
}

if (test == 3)
{
  if (lasR)
  {
    custom_function = function(z,i) { list(avgz = mean(z), avgi = mean(i)) }
    chm = rasterize(1, "max")
    met = rasterize(20, custom_function(Z, Intensity))
    den = rasterize(5, "count")
    pipeline = chm + met + den
    exec(pipeline, f)
  }
  else
  {
    custom_function = function(z,i) { list(avgz = mean(z), avgi = mean(i)) }
    ctg = readLAScatalog(f)
    chm = rasterize_canopy(ctg, 1, p2r())
    met = pixel_metrics(ctg, ~custom_function(Z, Intensity), 20)
    den = rasterize_density(ctg, 5)
  }
}

if (test == 4)
{
  if (lasR)
  {
    del = triangulate(filter = keep_ground())
    norm = transform_with(del)
    dtm = rasterize(1, del)
    chm = rasterize(1, "max")
    seed = local_maximum(3)
    tree = region_growing(chm, seed)
    write = write_las()
    pipeline = del + norm + write + dtm + chm + seed + tree
    ans = exec(pipeline, f, progress = TRUE, noread = TRUE)
  }
  else
  {
    ctg = readLAScatalog(f)

    opt_output_files(ctg) <- paste0(tempdir(), "/*_dtm")
    dtm = rasterize_terrain(ctg, 1, tin())

    opt_output_files(ctg) <- paste0(tempdir(), "/*_norm")
    nctg = normalize_height(ctg, tin())

    opt_output_files(nctg) <- paste0(tempdir(), "/chm_*")
    chm = rasterize_canopy(nctg, 1, p2r())
    chm = chm*1

    opt_output_files(nctg) <- ""
    seed = locate_trees(nctg, lmf(3), uniqueness = "gpstime")
    seed$treeID = 1:nrow(seed)

    tree = dalponte2016(chm, seed)()
  }
}

if (test == 5)
{
  if (lasR)
  {
    pipeline = normalize() + write_las()
    exec(pipeline, f, progress = TRUE, noread = TRUE)
  }
  else
  {
    ctg = readLAScatalog(f)
    opt_output_files(ctg) <- paste0(tempdir(), "/*_norm")
    norm = normalize_height(ctg, tin())
  }
}

if (test == 6)
{
  if (lasR)
  {
    pipeline = local_maximum(5)
    exec(pipeline, f, progress = TRUE, noread = TRUE)
  }
  else
  {
    ctg = readLAScatalog(f)
    tree = locate_trees(ctg, lmf(5))
  }
}

if (test == 7)
{
  if (lasR)
  {
    tri = triangulate()
    pipeline = reader_las(filter = keep_ground()) + tri + rasterize(1, tri)
    ans = exec(pipeline, f, progress = TRUE, noread = TRUE, chunk = 1000)
  }
  else
  {
    ctg = readLAScatalog(f)
    opt_chunk_size(ctg) = 1000
    dtm = rasterize_terrain(ctg, 1, tin())
  }
}

tf = Sys.time()
tf-ti

q("no")
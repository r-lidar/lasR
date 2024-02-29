#!/usr/bin/env Rscript
args = commandArgs(trailingOnly=TRUE)

if (length(args) != 2) {
  stop("At least 2 argument must be supplied", call.=FALSE)
} else {
  lasR = args[1] == "lasR"
  test = as.integer(args[2])
}

library(lidR)
library(lasR)

set_lidr_threads(half_cores())

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
    pipeline = reader(f) + rasterize(1, "max")
    processor(pipeline, progress = TRUE, noread = T)
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
    pipeline = reader(f, filter = keep_ground()) + tri + rasterize(1, tri)
    processor(pipeline)
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
    read = reader(f)
    chm = rasterize(1, "max")
    met = rasterize(20, custom_function(Z, Intensity))
    den = rasterize(5, "count")
    pipeline = read + chm + met + den
    processor(pipeline)
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
    read = reader(f)
    del = triangulate(filter = keep_ground())
    norm = transform_with(del)
    dtm = rasterize(1, del)
    chm = rasterize(1, "max")
    seed = local_maximum(3)
    tree = region_growing(chm, seed)
    write = write_las()
    pipeline = read + del + norm + write + dtm + chm + seed + tree
    ans = processor(pipeline, progress = TRUE)
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

tf = Sys.time()
tf-ti

q("no")
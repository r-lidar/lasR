test_that("growing region works",
{
  f = system.file("extdata", "MixedConifer.las", package="lasR")

  reader = reader(f, filter = "-keep_first")
  chm = rasterize(1, "max")
  lmx = local_maximum(5)
  tree = region_growing(chm, lmx, max_cr = 10)
  u = processor(reader + chm + lmx + tree)

  ttops = u[[2]]
  trees = u[[3]]
  id1 = sort(na.omit(as.numeric(unique(trees[]))))
  id2 = 1:nrow(ttops)-1

  expect_equal(id1, id2)
})

test_that("growing region works with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  reader = reader(f, filter = "-keep_first")
  chm = rasterize(1, "max")
  lmx = local_maximum(5)
  tree = region_growing(chm, lmx, max_cr = 10)
  u = processor(reader + chm + lmx + tree)
})

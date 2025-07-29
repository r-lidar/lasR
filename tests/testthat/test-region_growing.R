test_that("growing region works",
{
  f = system.file("extdata", "MixedConifer.las", package="lasR")

  reader = reader_las(filter = "-keep_first")
  chm = rasterize(1, "max")
  lmx = local_maximum(5)
  tree = region_growing(chm, lmx, max_cr = 10)
  u = exec(reader + chm + lmx + tree, on = f)

  chm = u[[1]]
  ttops = u[[2]]
  trees = u[[3]]
  id1 = sort(na.omit(as.numeric(unique(trees[]))))
  id2 = 1:nrow(ttops)-1

  expect_equal(id1, id2)

  skip_on_os("mac") # Cannot reproduce

  expect_equal(sum(!is.na(trees[])), 5967L)
})

test_that("growing region works with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  reader = reader_las(filter = keep_first())
  chm = rasterize(1, "max")
  lmx = local_maximum(5, min_height = 330)
  tree = region_growing(chm, lmx, max_cr = 10)

  u  = exec(reader + chm + lmx + tree, on = f)

  #terra::plot(u$rasterize, col = lidR::height.colors(25))
  #plot(u$local_maximum$geom, add = TRUE, cex = 0.1, pch = 19)
  #terra::plot(u$region_growing, col = lidR::random.colors(2000))
  #plot(u$local_maximum$geom, add = TRUE, cex = 0.1, pch = 19)

  expect_equal(length(unique(u$region_growing[])), 2235L) # 2234+NaN
  expect_gte(nrow(u$local_maximum), 2234L)
  expect_lte(nrow(u$local_maximum), 2235L)
  expect_equal(sum(is.na(u$rasterize[])), 5367L)

  # We have an issue on the r-universe version of macos-r-release it is 81 instead of 83
  # but on github action it is ok... who knows.
  #expect_gte(sum(is.na(u$region_growing[])), 6881)
  #expect_lte(sum(is.na(u$region_growing[])), 6883)
  skip_on_os("mac") # Cannot reproduce
  expect_equal(sum(is.na(u$region_growing[])), 15996L)
})


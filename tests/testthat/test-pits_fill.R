test_that("pits filling works",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")

  reader <- reader(f, filter = "-keep_first")
  tri <- triangulate()
  chm <- rasterize(0.25, tri)
  pit <- pit_fill(chm)
  u <- processor(reader + tri + chm + pit)
  chm <- u[[1]]
  sto <- u[[2]]

  expect_equal(mean(chm[], na.rm = T), 11.209, tolerance = 0.0001)
  expect_equal(mean(sto[], na.rm = T), 11.358, tolerance = 0.0001)
})

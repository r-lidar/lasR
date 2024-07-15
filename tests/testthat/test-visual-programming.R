test_that("drawflow pipelines works",
{
  f = system.file("extdata", "ui/json/two_simple_rasters.json", package="lasR")

  ans = exec(f)

  expect_equal(length(ans), 2)
  expect_equal(names(ans), c("rasterize", "rasterize.1"))
  expect_s4_class(ans[[1]], "SpatRaster")
  expect_s4_class(ans[[2]], "SpatRaster")
  expect_equal(dim(ans[[1]]), c(286,286,1))
  expect_equal(names(ans[[1]]), "z_max")
  expect_equal(dim(ans[[2]]), c(16,16,1))
  expect_equal(names(ans[[2]]), "z_mean")

  f = system.file("extdata", "ui/json/dtm_nornalize.json", package="lasR")

  ans = exec(f)

  expect_equal(length(ans), 2)
  expect_equal(names(ans), c("rasterize", "rasterize.1"))
  expect_s4_class(ans[[1]], "SpatRaster")
  expect_s4_class(ans[[2]], "SpatRaster")
  expect_equal(dim(ans[[1]]), c(286,286,1))
  expect_equal(dim(ans[[2]]), c(16,16,1))
  expect_equal(names(ans[[2]]), "z_mean")
})

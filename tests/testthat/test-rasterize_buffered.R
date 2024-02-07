test_that("Rasterize works with a moving window",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f)
  reg = rasterize(4, "count")
  win = rasterize(c(1,4), "count")
  pipeline = read + reg + win
  u = processor(pipeline)

  #terra::plot(u[[1]])
  #terra::plot(u[[2]])

  m1 = mean(u[[1]][], na.rm = TRUE)
  m2 = mean(u[[2]][], na.rm = TRUE)

  expect_equal(m1, 15.81, tolerance = 0.01)
  expect_equal(dim(u[[2]]), c(286, 286, 1))
  expect_equal(m1, m2, tolerance = 0.1)
  expect_equal(sum(is.na(u[[1]][])), 542)
  expect_equal(sum(is.na(u[[2]][])), 8760)
})

test_that("Rasterize works with a moving window",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f)
  reg = rasterize(4, length(Z))
  win = rasterize(c(1,4), length(Z))
  pipeline = read + reg + win
  u = processor(pipeline)

  #terra::plot(u[[1]])
  #terra::plot(u[[2]])

  m1 = mean(u[[1]][], na.rm = TRUE)
  m2 = mean(u[[2]][], na.rm = TRUE)

  expect_equal(m1, 15.81, tolerance = 0.01)
  expect_equal(dim(u[[2]]), c(286, 286, 1))
  expect_equal(m1, m2, tolerance = 0.1)
  expect_equal(sum(is.na(u[[1]][])), 542)
  expect_equal(sum(is.na(u[[2]][])), 8760)
})


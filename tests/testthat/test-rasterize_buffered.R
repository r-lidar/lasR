test_that("Rasterize works with a moving window",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  reg = rasterize(4, "count")
  win = rasterize(c(1,4), "count")
  pipeline = reg + win
  u = exec(pipeline, on = f)

  #terra::plot(u[[1]])
  #terra::plot(u[[2]])

  m1 = mean(u[[1]][], na.rm = TRUE)
  m2 = mean(u[[2]][], na.rm = TRUE)

  expect_equal(m1, 15.8128, tolerance = 1e-6)
  expect_equal(dim(u[[2]]), c(286L, 286L, 1))
  expect_equal(m1, m2, tolerance = 1e-1)
  expect_equal(sum(is.na(u[[1]][])), 542L)
  expect_equal(sum(is.na(u[[2]][])), 8760L)
})

test_that("Rasterize works with a moving window",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  reg = rasterize(4, length(Z))
  win = rasterize(c(1,4), length(Z))
  pipeline = reg + win
  u = exec(pipeline, on = f)

  #terra::plot(u[[1]])
  #terra::plot(u[[2]])

  m1 = mean(u[[1]][], na.rm = TRUE)
  m2 = mean(u[[2]][], na.rm = TRUE)

  expect_equal(m1, 15.8128, tolerance = 1e-6)
  expect_equal(dim(u[[2]]), c(286L, 286L, 1))
  expect_equal(m1, m2, tolerance = 0.1)
  expect_equal(sum(is.na(u[[1]][])), 542L)
  expect_equal(sum(is.na(u[[2]][])), 8760L)
})

test_that("Rasterize works with a moving window and multiple files",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  f = list.files(f, full.names = TRUE, pattern = "\\.laz")
  f = f[1:2]

  read = reader_las(filter = "-keep_every_nth 10")
  reg = rasterize(20, "zmean")
  win = rasterize(c(10,20), "zmean")
  pipeline = read + reg + win
  u = exec(pipeline, on = f)

  #terra::plot(u[[1]], col = lidR::height.colors(25))
  #terra::plot(u[[2]], col = lidR::height.colors(25))

  expect_s4_class(u[[1]], "SpatRaster")
  expect_s4_class(u[[2]], "SpatRaster")
  expect_equal(terra::res(u[[1]]), c(20,20))
  expect_equal(terra::res(u[[2]]), c(10,10))
  expect_equal(dim(u[[1]]), c(28L,10L,1L))
  expect_equal(dim(u[[2]]), c(55L,20L,1L))
  #expect_equal(names(chm), "")
  #expect_equal(names(sto), "")
  expect_equal(mean(u[[1]][], na.rm = T), 334.079, tolerance = 1e-6)
  expect_equal(mean(u[[2]][], na.rm = T), 334.072, tolerance = 1e-6)
  expect_equal(sum(is.na(u[[1]][])), 5L)
  expect_equal(sum(is.na(u[[2]][])), 2L)
})

test_that("reader_pcd works (binary in memory)",
{
  f <- system.file("extdata", "pcd_binary.pcd", package="lasR")
  pipeline = summarise() + rasterize(0.25, "max") + rasterize(0.25, "max", filter = "Z < 16") + delete_points(filter = "Z < 16") + summarise() + lasR:::nothing(T,F,F)
  ans = exec(pipeline, on = f)

  #terra::plot(ans[[2]])

  expect_false(lasR:::get_pipeline_info(pipeline)$streamable)

  expect_equal(ans[[1]]$npoints, 69977L)

  expect_equal(mean(ans[[2]][], na.rm = T), 16.746, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(mean(ans[[3]][], na.rm = T), 14.4109, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(ans[[4]]$npoints, 62598L)
})

test_that("reader_pcd works (binary in memory)",
{
  f <- system.file("extdata", "Example.pcd", package="lasR")
  fans = read_las(f)

  for (i in ncol(fans)) expect_true(is.numeric(fans[[i]]))
  expect_equal(mean(fans$gpstime), 269347.443)
  expect_equal(mean(fans$ScanAngle), -21.6666, tolerance = 1e-5)
  expect_equal(mean(fans$Intensity), 78.6)
})

test_that("reader_pcd works (ascii in memory)",
{
  f <- system.file("extdata", "pcd_ascii.pcd", package="lasR")
  pipeline = summarise() + rasterize(0.25, "max") + rasterize(0.25, "max", filter = "Z < 16") + delete_points(filter = "Z < 16") + summarise() + lasR:::nothing(T,F,F)
  ans = exec(pipeline, on = f)

  #terra::plot(ans[[2]])

  expect_false(lasR:::get_pipeline_info(pipeline)$streamable)

  expect_equal(ans[[1]]$npoints, 69977L)

  expect_equal(mean(ans[[2]][], na.rm = T), 16.74598, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(mean(ans[[3]][], na.rm = T), 14.41094, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(ans[[4]]$npoints, 62598L)
})

test_that("reader_pcd works (binary streamable)",
{
  f <- system.file("extdata", "pcd_binary.pcd", package="lasR")
  pipeline = summarise() + rasterize(0.25, "max") + rasterize(0.25, "max", filter = "Z < 16") + delete_points(filter = "Z < 16") + summarise()
  ans = exec(pipeline, on = f)

  #terra::plot(ans[[2]])

  expect_true(lasR:::get_pipeline_info(pipeline)$streamable)

  expect_equal(ans[[1]]$npoints, 69977L)

  expect_equal(mean(ans[[2]][], na.rm = T), 16.746, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(mean(ans[[3]][], na.rm = T), 14.4109, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(ans[[4]]$npoints, 62598L)
})

test_that("Fails with mixed format",
{
  f <- system.file("extdata", "pcd_binary.pcd", package="lasR")
  g <- system.file("extdata", "Example.las", package="lasR")

  expect_error(exec(lasR:::nothing(), on = c(f,g)), "Impossible to mix different file formats")
})

test_that("Fails with collection",
{
  f <- system.file("extdata", "pcd_binary.pcd", package="lasR")
  pipeline = reader() + summarise()

  expect_error({u = exec(pipeline, on = c(f, f))}, NA)
  expect_equal(u$npoints, 69977*2)

  f <- system.file("extdata", "pcd_binary.pcd", package="lasR")
  g <- system.file("extdata", "pcd_ascii.pcd", package="lasR")
  expect_error({u = exec(pipeline, on = c(f, g), buffer = 2)}, "PCD file reader cannot read buffered PCD files yet")
})
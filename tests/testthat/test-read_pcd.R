test_that("reader_pcd works (in memory)",
{
  f <- system.file("extdata", "nou11.pcd", package="lasR")
  pipeline = summarise() + rasterize(0.25, "max") + rasterize(0.25, "max", filter = "Z < 16") + delete_points(filter = "Z < 16") + summarise() + lasR:::nothing(T,F,F)
  ans = exec(pipeline, on = f)

  terra::plot(ans[[2]])

  expect_false(lasR:::get_pipeline_info(pipeline)$streamable)

  expect_equal(ans[[1]]$npoints, 69977L)

  expect_equal(mean(ans[[2]][], na.rm = T), 16.746, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(mean(ans[[3]][], na.rm = T), 14.4109, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 2)

  expect_equal(ans[[4]]$npoints, 62598L)
})


test_that("reader_pcd works (streamable)",
{
  f <- system.file("extdata", "nou11.pcd", package="lasR")
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

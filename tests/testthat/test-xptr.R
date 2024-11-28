test_that("external pointer works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  expect_error(ans <- lasR:::read_las(f), NA)

  u = exec(summarise(), on = ans)
  expect_equal(u$npoints, 73403)

  exec(sampling_pixel(2), on = ans)

  u = exec(summarise(), on = ans)
  expect_equal(u$npoints, 17182)

  u = exec(chm(5), on = ans)
  expect_s4_class(u, "SpatRaster")

  u = exec(local_maximum(5), on = ans)
  expect_s3_class(u, "sf")

  rm(ans)
  gc()
})

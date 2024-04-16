test_that("R convient filters work", {
  expect_equal(as.character(keep_class(c(5,7))), "-keep_class 5 7")
  expect_equal(as.character(drop_class(c(5,7))), "-drop_class 5 7")
  expect_equal(as.character(keep_first()), "-keep_first")
  expect_equal(as.character(drop_first()), "-drop_first")
  expect_equal(as.character(keep_ground()), "-keep_class 2")
  expect_equal(as.character(drop_ground()), "-drop_class 2")
  expect_equal(as.character(keep_noise()), "-keep_class 18")
  expect_equal(as.character(drop_noise()), "-drop_class 18")
  expect_equal(as.character(keep_z_above(7)), "-keep_z_above 7")
  expect_equal(as.character(keep_z_below(7)), "-keep_z_below 7")
  expect_equal(as.character(drop_z_above(7)), "-drop_z_above 7")
  expect_equal(as.character(drop_z_below(7)), "-drop_z_below 7")
  expect_equal(as.character(drop_duplicates()), "-drop_duplicates")
  expect_equal(as.character(keep_ground()+keep_z_above(7)), "-keep_class 2 -keep_z_above 7")
  expect_error(keep_ground()+ "-keep_class 2")

})

test_that("Usage works", {
  sink(tempfile())
  expect_error(filter_usage(), NA)
  expect_error(lasR:::transform_usage(), NA)
  expect_error(lasR:::available_threads(), NA)
  print(keep_first())
  sink(NULL)
})


test_that("rasterize works with a filter (#29)",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  imean1 <- rasterize(4, "imean", filter = drop_ground())
  imean2 <- rasterize(4, list(imean = mean(Intensity)), filter = drop_ground())
  pipeline = imean1+imean2
  ans = exec(pipeline, on = f)

  #terra::plot(ans$rasterize)
  #terra::plot(ans$aggregate)

  expect_equal(mean(ans$rasterize[], na.rm = T), 872.9656, tolerance = 1e-6)
  expect_equal(sum(is.na(ans$rasterize[])), 612)
  expect_equal(ans$rasterize[], ans$aggregate[])
})

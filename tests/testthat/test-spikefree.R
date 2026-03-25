test_that("Spikefree works",
{
  f <- system.file("extdata", "Megaplot.las", package="lasR")
  pipeline = spikefree(freeze_distance = 3)
  ans = exec(pipeline, on = f)

  expect_equal(mean(ans[], na.rm = TRUE), 14.91, tolerance = 0.001)
  expect_equal(sd(ans[], na.rm = TRUE), 7.943, tolerance = 0.001)
  expect_equal(sum(is.na(ans[])), 1027)
  #terra::plot(ans)
})

test_that("Spikefree works with a filter",
{
  f <- system.file("extdata", "Megaplot.las", package="lasR")
  pipeline = spikefree(freeze_distance = 3, filter = "Z > 4")
  ans = exec(pipeline, on = f)

  expect_equal(mean(ans[], na.rm = TRUE), 17.47, tolerance = 0.001)
  expect_equal(sd(ans[], na.rm = TRUE), 4.805, tolerance = 0.001)
  expect_equal(sum(is.na(ans[])), 24822)
  #terra::plot(ans)
})

test_that("Spikefree works with a filter",
{
  f <- system.file("extdata", "Megaplot.las", package="lasR")
  pipeline = spikefree(freeze_distance = 2)
  ans = exec(pipeline, on = f)

  expect_equal(mean(ans[], na.rm = TRUE), 14.285, tolerance = 0.001)
  expect_equal(sd(ans[], na.rm = TRUE), 7.886, tolerance = 0.001)
  expect_equal(sum(is.na(ans[])), 1027)
  #terra::plot(ans)
})
test_that("delete_points works (streaming)",
{
  f <- system.file("extdata", "Example.las", package="lasR")
  filter <- delete_points("Z <= 975")
  pipeline <- summarise() + filter + summarise()
  ans = exec(pipeline, f)

  expect_equal(ans[[1]]$npoints, 30)
  expect_equal(ans[[2]]$npoints, 19)
  expect_equal(ans[[1]]$z_histogram, c(`974` = 11, `976` = 11, `978` = 8))
  expect_equal(ans[[2]]$z_histogram, c(`976` = 11, `978` = 8))
})

test_that("delete_points works (batch)",
{
  f <- system.file("extdata", "Example.las", package="lasR")
  filter <- delete_points("Z <= 975")
  pipeline <- lasR:::nothing(read = TRUE) + summarise() + filter + summarise()
  ans = exec(pipeline, f)

  expect_equal(ans[[1]]$npoints, 30)
  expect_equal(ans[[2]]$npoints, 19)
  expect_equal(ans[[1]]$z_histogram, c(`974` = 11, `976` = 11, `978` = 8))
  expect_equal(ans[[2]]$z_histogram, c(`976` = 11, `978` = 8))
})

test_that("delete_points works when 0 point left # 46 (batch)",
{
  f <- system.file("extdata", "Example.las", package="lasR")
  filter <- delete_points("Z < 1000")
  pipeline <- lasR:::nothing(read = TRUE) + summarise() + filter + summarise()
  ans = exec(pipeline, f)

  expect_equal(ans[[1]]$npoints, 30)
  expect_equal(ans[[2]]$npoints, 0)
  expect_equal(ans[[1]]$z_histogram, c(`974` = 11, `976` = 11, `978` = 8))
  expect_equal(ans[[2]]$z_histogram, c(`0` = 0))
})

test_that("delete point memory reallocation works",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  pipeline = delete_points("Classification != 2") + geometry_features(k = 15 , features = "lps") + write_las()
  expect_error(exec(pipeline, on = f), NA)
})


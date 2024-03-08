test_that("delete_points works (streaming)",
{
  f <- system.file("extdata", "Example.las", package="lasR")
  filter <- delete_points(keep_z_above(975))
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
  filter <- delete_points(keep_z_above(975))
  pipeline <- lasR:::nothing(read = TRUE) + summarise() + filter + summarise()
  ans = exec(pipeline, f)

  expect_equal(ans[[1]]$npoints, 30)
  expect_equal(ans[[2]]$npoints, 19)
  expect_equal(ans[[1]]$z_histogram, c(`974` = 11, `976` = 11, `978` = 8))
  expect_equal(ans[[2]]$z_histogram, c(`976` = 11, `978` = 8))
})

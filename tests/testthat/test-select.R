test_that("select with grid works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  pipeline = filter_with_grid(10, "max") + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$npoints, 848)
  expect_equal(ans$z_histogram[11], c(`816` = 104))

  pipeline = filter_with_grid(5, "min") + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$npoints, 3042)
  expect_equal(ans$z_histogram[11], c(`808` = 420))

  pipeline = filter_with_grid(10, "max", keep_ground()) + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$npoints, 789)
  expect_equal(ans$z_histogram[11], c(`810` = 115))
  expect_equal(ans$npoints_per_class, c(`2` = 789))
})

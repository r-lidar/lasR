test_that("select with grid works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  pipeline = filter_with_grid(10, "max") + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$npoints, 848)
  expect_equal(as.numeric(names(ans$z_histogram[11])), c(816))
  expect_equal(unname(ans$z_histogram[11]), c(112))

  pipeline = filter_with_grid(5, "min") + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$npoints, 3042)
  expect_equal(as.numeric(names(ans$z_histogram[11])), c(808))
  expect_equal(unname(ans$z_histogram[11]), c(406))

  pipeline = filter_with_grid(10, "max", keep_ground()) + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$npoints, 789)
  expect_equal(as.numeric(names(ans$z_histogram[11])), c(808))
  expect_equal(unname(ans$z_histogram[11]), c(148))

})

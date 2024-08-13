test_that("CSF works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  pipeline = classify_with_csf(TRUE, 1 ,1, time_step = 1, filter = "") + summarise()
  ans = exec(pipeline, on = f)
  ans$npoints_per_class

  class = c(226,45282,23998,3897)
  names(class) = c(0,1,2,9)
  expect_equal(ans$npoints_per_class, class, tolerance = 1)
})

test_that("CSF works with a filter",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  pipeline = classify_with_csf(TRUE, 1 ,1, time_step = 1) + summarise()
  ans = exec(pipeline, on = f)
  ans$npoints_per_class

  class = c(175, 46509, 22822, 3897)
  names(class) = c(0,1,2,9)
  expect_equal(ans$npoints_per_class, class, tolerance = 1)
})
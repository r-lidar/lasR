test_that("PTD works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  pipeline = classify_with_ptd() + summarise()
  ans = exec(pipeline, on = f, ncores = 6)

  class = c(47875,24260,1023,46)
  names(class) = c(1,2,7,9)
  expect_equal(ans$npoints_per_class, class, tolerance = 1)
})

test_that("PTD works with a filter",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  pipeline = classify_with_ptd(filter = "Intensity > 1200") + summarise()
  ans = exec(pipeline, on = f)
  class = c(56915, 14176, 669, 1643)
  names(class) = c(1,2,7,9)
  expect_equal(ans$npoints_per_class, class, tolerance = 1)
})

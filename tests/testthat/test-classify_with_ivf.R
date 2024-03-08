test_that("classify noise with ivf works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_isolated_points()
  expect_error(exec(class, f), NA)
})

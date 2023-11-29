test_that("classify noise with ivf works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  read = reader(f)
  class = classify_isolated_points()
  expect_error(processor(read + class))
})

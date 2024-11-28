test_that("sort works", {
  f <- system.file("extdata", "Topography.las", package="lasR")
  expect_error(exec(sort_points(), on = f), NA)
})

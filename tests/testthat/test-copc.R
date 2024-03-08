test_that("works with COPC",
{
  f = system.file("extdata", "example.copc.laz", package="lasR")
  expect_warning(exec(lasR:::nothing(read = T), f), NA)
})

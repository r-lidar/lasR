test_that(
{
  f = system.file("extdata", "example.copc.laz", package="lasR")
  read = reader(f)
  expect_warning(processor(read), NA)
})

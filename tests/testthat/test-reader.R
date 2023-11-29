f = system.file("extdata", "Example.las", package="lasR")

test_that("reader works with single file",
{
  pipeline = reader(f)

  expect_error({u = processor(pipeline)}, NA)
  expect_length(u, 0)
})

test_that("reader works with multiple files",
{
  pipeline = reader(c(f, f))

  expect_error({u = processor(pipeline)}, NA)
  expect_length(u, 0)
})

test_that("reader fails if file does not exist",
{
  expect_error(reader("missing.las"))
})

test_that("fails if wrong extension",
{
  f = system.file("extdata", "Example.las", package="lasR")
  pipeline = reader(f) + rasterize(2, ofile = tempfile(fileext = ".bla"))
  expect_error(processor(pipeline), "file extension 'bla' not registered in the database")

  pipeline = reader(f) + rasterize(2, ofile = tempfile(fileext = ""))
  expect_error(processor(pipeline), "cannot find file extension")
})

test_that("fails if wrong extension",
{
  f = system.file("extdata", "Example.las", package="lasR")
  pipeline = rasterize(2, ofile = tempfile(fileext = ".bla"))
  expect_error(exec(pipeline, f), "file extension 'bla' not registered in the database")

  pipeline = rasterize(2, ofile = tempfile(fileext = ""))
  expect_error(exec(pipeline, f), "cannot find file extension")
})

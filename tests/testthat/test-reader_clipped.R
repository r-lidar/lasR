
test_that("reader_circle works (#141)",
{
  file <- c(system.file("extdata", "MixedConifer.las", package="lasR"))

  read <- lasR::reader_circles(xc = 481305, yc = 3812966, r = 50)
  pipeline = read + summarise() + lasR::write_las(ofile = paste0(tempfile(), ".las"))

  res1 <- lasR::exec(pipeline, on = file, buffer = 0)
  res2 <- lasR::exec(pipeline, on = file, buffer = 30)

  res1 = read_las(res1$write_las)
  res2 = read_las(res2$write_las)

  expect_equal(res1, res2)
  expect_equal(dim(res1), c(29488, 18))
})

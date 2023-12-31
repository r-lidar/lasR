test_that("prints works with complex pipeline",
{
  count = function(data) { length(data$X) }
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  read = reader(f)
  del = triangulate(filter = keep_ground())
  npts = callback(count, expose = "x")
  sum = summarise()
  pipeline = read + del + npts + sum

  sink(tempfile())
  expect_error(print(pipeline), NA)
  sink(NULL)
})


test_that("convenient filters work",
{
  filter = keep_class(c(2,4)) + keep_z_below(2)
  expect_equal(as.character(filter), "-keep_class 2 4 -keep_z_below 2")
})

test_that("filter_usage works",
{
  sink(tempfile())
  expect_error(filter_usage(), NA)
  sink(NULL)
})


test_that("prints works with complex pipeline",
{
  count = function(data) { length(data$X) }
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, full.names = T)
  read = reader(f)
  del = triangulate(filter = keep_ground())
  met = rasterize(1, mean(Z))
  npts = callback(count, expose = "x")
  hull = hulls(del)
  sum = summarise()
  pipeline = read + del + npts + sum + hull

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


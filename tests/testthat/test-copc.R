test_that("works with COPC",
{
  f = system.file("extdata", "example.copc.laz", package="lasR")
  expect_warning(exec(lasR:::nothing(read = T), f), NA)
})

test_that("works with COPC",
{
  skip("Work on COPC")

  f = system.file("extdata", "Topography.las", package="lasR")
  o = paste0(tempdir(), "/test.copc.laz")

  pipeline = write_las(o)
  ans = exec(pipeline, on = f)

  expect_equal(ans, o)

  pipeline = reader(copc_depth = 0) + summarise()
  ans = exec(pipeline, on = "/home/jr/Documents/Ulaval/Projets Annexes/LAStools - copc/project copc/copc/autzen-classified.copc.laz")
  expect_equal(ans$npoints, 73402)
})
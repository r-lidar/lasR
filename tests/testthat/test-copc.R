test_that("works with COPC",
{
  f = system.file("extdata", "example.copc.laz", package="lasR")
  expect_warning(exec(lasR:::nothing(read = T), f), NA)
})

test_that("Max depth in COPC",
{
  skip_if(Sys.info()[["user"]] != "jr")

  f = "/home/jr/R packages/r-lidar/lasR/inst/extdata/autzen-classified.copc.laz"

  pipeline = reader(copc_depth = 0) + summarise()
  ans = exec(pipeline, on = f)
  expect_equal(ans$npoints, 61201L)

  pipeline = reader_las(copc_depth = 1) + summarise()
  ans = exec(pipeline, on = f)
  expect_equal(ans$npoints, 131060L)
})

test_that("works with COPC",
{
     f = system.file("extdata", "Megaplot.las", package="lasR")
     o = tempfile(fileext = ".copc.laz")
     expect_error(exec(write_copc(o), f), NA)
})

test_that("Write COPC",
{
  skip_if(Sys.info()[["user"]] != "jr")

  f = "/home/jr/R packages/r-lidar/lasR/inst/extdata/autzen-classified.copc.laz"
  o = tempfile(fileext = ".copc.laz")

  pipeline = reader(copc_depth = 2) + write_las(o)
  ans = exec(pipeline, on = f)

  pipeline = reader_las(copc_depth = 0) + summarise()
  ans = exec(pipeline, on = o)
  expect_equal(ans$npoints, 91876L, tolerance = 0.0001)

  pipeline = reader_las(copc_depth = 1) + summarise()
  ans = exec(pipeline, on = o)
  expect_equal(ans$npoints, 400000L, tolerance = 0.0008)

  pipeline = reader_las(copc_depth = 2) + summarise()
  ans = exec(pipeline, on = o)
  expect_equal(ans$npoints, 577636L, tolerance = 0.000002) # 36 or 37

  pipeline = reader_las() + summarise()
  ans = exec(pipeline, on = o)
  expect_equal(ans$npoints, 577636L, tolerance = 0.000002) # 36 or 37
})

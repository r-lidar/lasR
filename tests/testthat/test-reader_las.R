f = system.file("extdata", "Example.las", package="lasR")

test_that("reader works with single file",
{
  pipeline = reader_las()

  expect_error({u = exec(pipeline, on = f)}, NA)
  expect_length(u, 0)

  pipeline = reader_las(filter = "-keep_intensity_above 100") + summarise()

  expect_error({u = exec(pipeline, on = f)}, NA)
  expect_equal(u$i_histogram, c("100" = 5))

  pipeline = reader_las(filter = "Intensity > 50") + summarise()

  expect_error({u = exec(pipeline, on = f)}, NA)
  expect_equal(u$i_histogram, c("50" = 5, "100" = 21))
})

test_that("reader works with multiple files",
{
  pipeline = reader_las()

  expect_error({u = exec(pipeline, on = c(f,f))}, NA)
  expect_length(u, 0)
})

test_that("reader works with multiple files and filter",
{
  pipeline = reader_las(filter = keep_ground()) + summarise()

  expect_error({u = exec(pipeline, on = c(f,f))}, NA)
  expect_equal(names(u$npoints_per_class), "2")
})

test_that("reader fails if file does not exist",
{
  p = reader_las()
  expect_error(exec(p, on = "missin.las"), "File not found")

  f = system.file("extdata", "bcts/bcts.gpkg", package="lasR")
  expect_error(exec(p, on = f), "Unknown file type")

  f = system.file("extdata", "bcts/bcts.vpc", package="lasR")
  g = system.file("extdata", "Megaplot.las", package="lasR")
  f = c(f, g)
  expect_error(exec(reader_las(), on = f), "Virtual point cloud file detected mixed with other content")
})

test_that("reader can read a folder",
{
  f = system.file("extdata", "bcts/", package="lasR")
  p <- reader_las() + hulls()
  ans = exec(p, on = f)
  expect_equal(dim(ans), c(4L,1L))
})

test_that("read_las works (in memory)",
{
  f <- system.file("extdata", "Megaplot.las", package="lasR")
  pipeline = summarise() + rasterize(2, "max") + rasterize(2, "max", filter = "Z < 16") + delete_points(filter = "Z < 16") + summarise() + lasR:::nothing(T,F,F)
  ans = exec(pipeline, on = f)

  expect_false(lasR:::get_pipeline_info(pipeline)$streamable)

  expect_equal(ans[[1]]$npoints, 81590L)

  expect_equal(mean(ans[[2]][], na.rm = T), 16.2466, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 559L)

  expect_equal(mean(ans[[3]][], na.rm = T), 10.52907, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[3]][])), 2588L)

  expect_equal(ans[[4]]$npoints, 44974L)
})


test_that("reader_las works (streamable)",
{
  f <- system.file("extdata", "Megaplot.las", package="lasR")
  pipeline = summarise() + rasterize(2, "max") + rasterize(2, "max", filter = "Z < 16") + delete_points(filter = "Z < 16") + summarise()
  ans = exec(pipeline, on = f)

  expect_true(lasR:::get_pipeline_info(pipeline)$streamable)

  expect_equal(ans[[1]]$npoints, 81590L)

  expect_equal(mean(ans[[2]][], na.rm = T), 16.2466, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 559L)

  expect_equal(mean(ans[[3]][], na.rm = T), 10.52907, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[3]][])), 2588L)

  expect_equal(ans[[4]]$npoints, 44974L)
})


test_that("reader_las works (filter case sensitive)",
{
  f <- system.file("extdata", "Megaplot.las", package="lasR")
  pipeline = summarise() + rasterize(2, "max") + rasterize(2, "max", filter = "z < 16") + delete_points(filter = "z < 16") + summarise()
  ans = exec(pipeline, on = f)

  expect_true(lasR:::get_pipeline_info(pipeline)$streamable)

  expect_equal(ans[[1]]$npoints, 81590L)

  expect_equal(mean(ans[[2]][], na.rm = T), 16.2466, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[2]][])), 559L)

  expect_equal(mean(ans[[3]][], na.rm = T), 10.52907, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[[3]][])), 2588L)

  expect_equal(ans[[4]]$npoints, 44974L)
})



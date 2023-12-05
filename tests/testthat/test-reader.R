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

test_that("reader_dataframe works",
{
  f = system.file("extdata", "Example.rds", package="lasR")
  las = readRDS(f)

  pipeline = reader(las)

  expect_error(processor(pipeline), NA)

  pipeline2 = pipeline + hulls()

  ans = processor(pipeline2)

  expect_s3_class(ans, "sf")
  expect_equal(nrow(ans), 1L)

  pipeline3 = pipeline + rasterize(0.5)

  ans = processor(pipeline3)

  expect_s4_class(ans, "SpatRaster")
  expect_equal(dim(ans), c(3L,26L,1L))

  pipeline4 = pipeline + local_maximum(3)

  ans = processor(pipeline4)

  expect_s3_class(ans, "sf")
  expect_equal(nrow(ans), 3L)

  pipeline5 = reader(las, xmin = 339008, ymin = 0, xmax = 339011, ymax = 1e8) + summarise()
  ans = processor(pipeline5)

  expect_equal(ans$npoints, 17L)
})


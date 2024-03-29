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
  expect_warning(p <- reader("missing.las"))
  expect_error(processor(p), "File not found")

  f = system.file("extdata", "bcts/bcts.gpkg", package="lasR")
  expect_error(processor(reader(f)), "Unknown file type")

  f = system.file("extdata", "bcts/bcts.vpc", package="lasR")
  g = system.file("extdata", "Megaplot.las", package="lasR")
  f = c(f, g)
  expect_error(processor(reader(f)), "Virtual point cloud file detected mixed with other content")
})

test_that("reader can read a folder",
{
  f = system.file("extdata", "bcts/", package="lasR")
  p <- reader(f) + hulls()
  ans = processor(p)
  expect_equal(dim(ans), c(4L,1L))
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

  pipeline6 = reader(las, xc = 339008, yc = 5248000, r = 2) + summarise()
  ans = processor(pipeline6)

  expect_equal(ans$npoints, 15L)
})

read_las = function(files, select = "*", filter = "")
{
  load = function(data) { return(data) }
  read = reader(files, filter = filter)
  call = callback(load, expose = select, no_las_update = TRUE)
  return(processor(read+call))
}

test_that("reader_dataframe works with extrabytes",
{
  f = system.file("extdata", "Example.rds", package="lasR")
  las = readRDS(f)
  las$foo = 1L
  las$bar = 2.5
  read = reader(las)
  write = write_las()

  expect_error(ans <- processor(read+write), NA)
  expect_equal(basename(ans), "data.frame.las")
  las = read_las(ans)
  expect_true(all(c("foo", "bar") %in% names(las)))
  expect_true(las$foo[1] == 1L)
  expect_true(is.integer(las$foo))
  expect_true(las$bar[1] == 2.5)
  expect_false(is.integer(las$bar))
})

test_that("reader_dataframe works with RGB",
{
  f = system.file("extdata", "Example.rds", package="lasR")
  las = readRDS(f)
  las$R = 10L
  las$G = 20L
  las$B = 30L
  read = reader(las)
  write = write_las()

  expect_error(ans <- processor(read+write), NA)
  expect_equal(basename(ans), "data.frame.las")
  expect_true(file.exists(ans))

  las = read_las(ans)

  expect_true(all(c("R", "G", "B") %in% names(las)))
  expect_equal(las$R[1], 10L)
  expect_equal(las$G[1], 20L)
  expect_equal(las$B[1], 30L)
})

test_that("reader_dataframe works with NIR",
{
  # there is an issue with point format 8 both in rlas ans lasR.
  skip("The problem is also in rlas")

  f = system.file("extdata", "Example.rds", package="lasR")
  las = readRDS(f)
  las$NIR = 10L
  read = reader(las)
  write = write_las()

  expect_error(ans <- processor(read+write), NA)
  expect_equal(basename(ans), "data.frame.las")
  expect_true(file.exists(ans))

  las = read_las(ans)

  expect_true(all(c("R", "G", "B", "NIR") %in% names(las)))
  expect_equal(las$R[1], 0L)
  expect_equal(las$G[1], 0L)
  expect_equal(las$B[1], 0L)
  expect_true(las$NIR[1], 10L)
})

test_that("reader_dataframe propagates the CRS",
{
  wkt = sf::st_crs(26917)$wkt
  f = system.file("extdata", "Example.rds", package="lasR")
  las = readRDS(f)
  attr(las, "crs") = wkt
  read = reader(las)
  rast = rasterize(2)
  u = processor(read+rast)

  expect_equal(terra::crs(u), wkt)
})

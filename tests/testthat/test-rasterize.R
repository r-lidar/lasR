test_that("rasterize streamed works",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f, filter = "")
  met = rasterize(5, c("min", "max", "count"))
  pipeline = read + met
  u = processor(pipeline)

  expect_s4_class(u, "SpatRaster")
  expect_equal(names(u), c("min", "max", "count"))
  expect_equal(dim(u), c(58, 58, 3))
  expect_equal(mean(u[[1]][], na.rm = T), 804.9969, tolerance = 1e-6)
  expect_equal(mean(u[[2]][], na.rm = T), 813.256, tolerance = 1e-6)
  expect_equal(mean(u[[3]][], na.rm = T), 24.12985, tolerance = 1e-6)
})

test_that("rasterize non streamed works",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f, filter = "")
  met = rasterize(5, c("count", "zmax", "zmin", "zmean", "zmedian", "zsd", "zcv", "zp50", "imax", "imin", "imean", "imedian", "isd", "icv", "ip100"))
  pipeline = read + met
  u = processor(pipeline)

  #terra::plot(u, col = lidR::height.colors(25))

  expect_s4_class(u, "SpatRaster")
  expect_equal(names(u),  c("count", "zmax", "zmin", "zmean", "zmedian", "zsd", "zcv", "zp50", "imax", "imin", "imean", "imedian", "isd", "icv", "ip100"))
  expect_equal(dim(u), c(58, 58, 15))
  expect_equal(mean(u[[1]][], na.rm = T), 24.12985, tolerance = 1e-6) # count
  expect_equal(mean(u[[2]][], na.rm = T), 813.256, tolerance = 1e-6) # zmax
  expect_equal(mean(u[[3]][], na.rm = T), 804.9969, tolerance = 1e-6) # zmin
  expect_equal(mean(u[[9]][], na.rm = T), 1380.564, tolerance = 1e-6) # imax
  expect_equal(mean(u[[12]][], na.rm = T), 942.822, tolerance = 1e-6) # imedian
  expect_equal(u[[5]][], u[[8]][], ignore_attr = TRUE) # median = percentile 50
  expect_equal(u[[9]][], u[[15]][], ignore_attr = TRUE) # max = percentile 100
})

test_that("rasterize expression works",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f, filter = "")
  met = rasterize(5, mean(Intensity))
  pipeline = read + met
  u = processor(pipeline)

  expect_s4_class(u, "SpatRaster")
  expect_equal(dim(u), c(58, 58, 1))
  expect_equal(mean(u[], na.rm = T), 904.078, tolerance = 1e-6)
})

test_that("rasterize expression works multiband",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  myMetric = function(z) { return(list(a = mean(z), b = max(z))) }
  read = reader(f, filter = keep_first())
  met = rasterize(5, myMetric(Z))
  pipeline = read + met
  u = processor(pipeline)

  expect_s4_class(u, "SpatRaster")
  expect_equal(dim(u), c(58, 58, 2))
  expect_equal(names(u),c("a", "b"))
 })

test_that("rasterize expression works with extrabytes",
{
  f = system.file("extdata", "extra_byte.las", package="lasR")

  read = reader(f, filter = "")
  met = rasterize(5, mean(Amplitude))
  pipeline = read + met
  u = suppressWarnings(processor(pipeline))

  expect_s4_class(u, "SpatRaster")
  expect_equal(dim(u), c(2, 5, 1))
  expect_equal(mean(u[], na.rm = T), 9.1736, tolerance = 1e-5)
})


test_that("rasterize captures the expression",
{
  f = system.file("extdata", "Example.las", package="lasR")

  fun = function(x) {return(mean(x))}
  read = reader(f, filter = "")
  met = rasterize(5, fun(Intensity))
  pipeline = read + met
  expect_error(processor(pipeline), NA)
})

test_that("rasterize captures the expression and handles error",
{
  f = system.file("extdata", "Example.las", package="lasR")

  fun = function(x) {return(list(a = mean(x), b = c(0,0)))}
  read = reader(f, filter = "")
  met = rasterize(5, fun(Intensity))
  pipeline = read + met
  expect_error(processor(pipeline), "must only return a vector of numbers or a list of atomic numbers")

  fun = function(x) {return(list(list(a = mean(x))))}
  read = reader(f, filter = "")
  met = rasterize(5, fun(Intensity))
  pipeline = read + met
  expect_error(processor(pipeline), "user expression must only return numbers")
 })


test_that("rasterize triangulation works with 1 file",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f, filter = "-keep_class 2")
  tri = triangulate(15)
  dtm = rasterize(5, tri)
  pipeline = read + tri + dtm
  u = processor(pipeline)

  expect_s4_class(u, "SpatRaster")
  expect_equal(dim(u), c(58, 58, 1))
  expect_equal(mean(u[], na.rm = T), 805.4077, tolerance = 1e-6)
  expect_equal(sum(is.na(u[])), 713)
  #terra::plot(u[[2]])
})


test_that("rasterize triangulation works with 4 files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader(f, filter = "-keep_class 2")
  tri = triangulate(15)
  dtm = rasterize(5, tri)
  pipeline = read + tri + dtm
  u = processor(pipeline)

  dtm = u
  expect_s4_class(dtm, "SpatRaster")
  expect_equal(dim(dtm), c(213, 42, 1))

  payload = u[]
  expect_equal(mean(payload, na.rm = TRUE), 341.4066, tolerance = 1e-6)
  expect_equal(sum(is.na(payload)), 876)
  #terra::plot(u)
})

test_that("rasterize splits the raster on demand",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  ofile1 = paste0(tempdir(), "/chm_*.tif")
  ofile2 = paste0(tempdir(), "/chm.tif")

  x = c(885100, 885100)
  y = c(629200, 629300)
  r = 20

  pipeline1 = reader(f, xc = x, yc = y, r = r) + rasterize(2, "max", ofile = ofile1)
  pipeline2 = reader(f, xc = x, yc = y, r = r) + rasterize(2, "max", ofile = ofile2)

  ans1 = processor(pipeline1)
  ans2 = processor(pipeline2)

  expect_equal(length(ans1), 2L)
  expect_s4_class(ans2, "SpatRaster")
  expect_equal(basename(ans1), c("chm_bcts_1_0.tif", "chm_bcts_1_1.tif"))
  expect_equal(dim(ans2), c(272L,98L,1L))

  r1 = terra::rast(ans1[1])
  r2 = terra::rast(ans1[2])

  expect_equal(dim(r1), c(21L,21L,1L))
  expect_equal(dim(r2), c(21L,21L,1L))
  expect_equal(mean(r1[], na.rm = T), 338.619, tolerance = 0.00001)
  expect_equal(mean(r2[], na.rm = T), 337.441, tolerance = 0.00001)
})

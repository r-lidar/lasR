test_that("triangulate works",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader_las(filter = "-keep_class 2")
  tri = triangulate(15, ofile = tempfile(fileext = ".gpkg"))
  u = exec(read + tri, on = f)

  expect_s3_class(u, "sf")
  expect_equal(length(u$geom[[1]]), 16025)
  #plot(u)
})

test_that("triangulate works with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader_las(filter = "-keep_class 2 -drop_random_fraction 0.5")
  tri = triangulate(15, ofile = tempfile(fileext = ".gpkg"))
  u = exec(read + tri, on = f)

  expect_s3_class(u, "sf")
  expect_equal(nrow(u), 4)
  expect_equal(length(u$geom[[1]]), 97392)
  #plot(u)
})

test_that("triangulate works with intensity",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader_las(filter = "-keep_class 2 -drop_random_fraction 0.5")
  tri = triangulate(15, use_attribute = "Intensity", ofile = tempgpkg())
  rast = rasterize(5, tri)
  u = exec(read + tri + rast, on = f)

  expect_equal(range(u$rasterize[], na.rm = T), c(191.3, 2033.3), tolerance = 0.01)
})

test_that("triangulate works with gpstime",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader_las(filter = "-keep_class 2 -drop_random_fraction 0.5")
  tri = triangulate(15, use_attribute = "gpstime", ofile = tempgpkg())
  rast = rasterize(5, tri)
  u = exec(read + tri + rast, on = f)

  expect_equal(range(u$rasterize[], na.rm = T), c(220367376.0, 220367392.0), tolerance = 0.01)
})


test_that("interpolation fails with 0 points",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  mesh  = triangulate(50, filter = keep_class(24))
  trans = transform_with(mesh)
  rr = rasterize(2, mesh)

  expect_error(exec(mesh + trans, on = f), NA)

  ans = exec(mesh + rr, on = f)

  expect_s4_class(ans, "SpatRaster")
  expect_equal(dim(ans), c(144L, 144L, 1L))
  expect_true(all(is.na(ans[])))
})

#test_that("triangulate fails with 0 points (#25)",
#{
#  f <- system.file("extdata", "las14_pdrf6.laz", package="lasR")
#  pipeline <- reader_las() + dtm()
#  expect_error(suppressWarnings(exec(pipeline, on = f)), "impossible to construct a Delaunay triangulation with 0 points")
#})

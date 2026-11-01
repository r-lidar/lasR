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

test_that("interpolation ignores degenerated triangles on collinear hull edges (#355)",
{
  # Flat 4x4 grid: collinear points on the hull produce zero-area triangles
  # that used to interpolate to NaN. Every height must be exactly 0.
  xy = expand.grid(X = c(1, 19, 21, 39), Y = c(1, 19, 21, 39))
  data = data.frame(xy, Z = 10, Classification = 2L)

  pipeline = normalize() + callback(function(x) x, expose = "xyz", no_las_update = TRUE)

  # The triangulation includes random noise: repeat to catch the bug
  for (i in 1:20)
  {
    res = exec(pipeline, on = data, with = list(progress = FALSE))
    expect_equal(nrow(res), 16L)
    expect_true(all(is.finite(res$Z)))
    expect_true(all(res$Z == 0))
  }

  # Same with the raster branch
  tri = triangulate()
  pipeline = tri + rasterize(1, tri)
  for (i in 1:20)
  {
    r = exec(pipeline, on = data, with = list(progress = FALSE))
    v = terra::values(r)
    v = v[!is.na(v)]
    expect_true(length(v) > 0)
    expect_true(all(v == 10))
  }
})

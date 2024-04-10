test_that("triangulate works",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f, filter = "-keep_class 2")
  tri = triangulate(15, ofile = tempfile(fileext = ".gpkg"))
  u = processor(read + tri)

  expect_s3_class(u, "sf")
  expect_equal(length(u$geom[[1]]), 16025)
  #plot(u)
})

test_that("triangulate works with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader(f, filter = "-keep_class 2 -drop_random_fraction 0.5")
  tri = triangulate(15, ofile = tempfile(fileext = ".gpkg"))
  u = processor(read + tri)

  expect_s3_class(u, "sf")
  expect_equal(nrow(u), 4)
  expect_equal(length(u$geom[[1]]), 97392)
  #plot(u)
})


test_that("triangulate works with intensity",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  read = reader(f, filter = "-keep_class 2 -drop_random_fraction 0.5")
  tri = triangulate(15, use_attribute = "Intensity", ofile = tempgpkg())
  rast = rasterize(5, tri)
  u = processor(read + tri + rast)

  expect_equal(range(u$rasterize[], na.rm = T), c(165.72, 2368.14), tolerance = 0.01)
})


test_that("triangulate fails with 0 points (#25)",
{
  f <- system.file("extdata", "las14_pdrf6.laz", package="lasR")
  pipeline <- reader_las() + dtm()
  expect_error(suppressWarnings(exec(pipeline, on = f)), "impossible to construct a Delaunay triangulation with 0 points")
})

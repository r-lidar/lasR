test_that("tw can normalize",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  mesh  = triangulate(50)
  norm = mesh + transform_with(mesh)
  pipeline = reader(f, filter = "-keep_class 2") + norm + write_las()
  g = processor(pipeline)

  pipeline = reader(g, filter = "-keep_class 2") + summarise()
  u = processor(pipeline)

  expect_equal(u$z_histogram, c("0" = 8159))
})

test_that("tw can normalize with extrabyte and sets scale and offsets",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  mesh  = triangulate(50, filter = keep_ground())
  exby = add_extrabytes("int", "HAG", "Height Above Ground")
  norm = transform_with(mesh, store_in_attribute = "HAG")
  pipeline = reader(f) + mesh + exby + norm + write_las()
  suppressWarnings(g <- processor(pipeline))

  pipeline = reader(g) + callback(function(data) { return(data$HAG) }, expose = "0")
  u = processor(pipeline)
  expect_equal(range(u), c(-2.476, 20.977), tolerance = 0.0005)
})

test_that("tw can normalize with a raster",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  mesh  = triangulate(0, filter = keep_ground())
  dtm = rasterize(1, mesh, ofile = "")
  norm = transform_with(dtm)
  pipeline = reader(f) + mesh + dtm + norm + write_las()
  suppressWarnings(g <- processor(pipeline))

  pipeline = reader(g, filter = keep_ground()) + callback(function(data) { return(data$Z) }, expose = "z")
  u = processor(pipeline)
  expect_equal(mean(u), 0, tolerance = 0.01)
  expect_equal(sd(u), 0.138, tolerance = 0.003)
})


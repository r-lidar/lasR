test_that("tw can normalize",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  mesh  = triangulate(50)
  norm = mesh + transform_with(mesh)
  pipeline = reader_las(filter = "-keep_class 2") + norm + write_las()
  g = exec(pipeline, on = f)

  pipeline = reader_las(filter = "-keep_class 2") + summarise()
  u = exec(pipeline, on = g)

  expect_equal(u$z_histogram, c("0" = 8159))
})

test_that("tw can normalize with extrabyte and sets scale and offsets",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  mesh  = triangulate(50, filter = keep_ground())
  exby = add_extrabytes("int", "HAG", "Height Above Ground")
  norm = transform_with(mesh, store_in_attribute = "HAG")
  pipeline = reader_las() + mesh + exby + norm + write_las()
  suppressWarnings(g <- exec(pipeline, on = f))

  pipeline = reader_las() + callback(function(data) { return(data$HAG) }, expose = "0")
  u = exec(pipeline, on = g)
  expect_equal(range(u), c(-2.476, 20.977), tolerance = 0.0005)
})

test_that("tw can normalize with a raster",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  mesh  = triangulate(0, filter = keep_ground())
  dtm = rasterize(1, mesh, ofile = "")
  norm = transform_with(dtm)
  pipeline = reader_las() + mesh + dtm + norm + write_las()
  suppressWarnings(g <- exec(pipeline, on = f))

  pipeline = reader_las(filter = keep_ground()) + callback(function(data) { return(data$Z) }, expose = "z")
  u = exec(pipeline, on = g)
  expect_equal(mean(u), 0, tolerance = 0.01)
  expect_equal(sd(u), 0.138, tolerance = 0.003)
})

test_that("tw can normalize with extrabyte",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  olas = templas()
  pipeline <- reader_las() + hag() + write_las(olas)
  ans = exec(pipeline, on = f) |> suppressWarnings()

  del = triangulate(use_attribute = "HAG")
  dtm = rasterize(2, del)
  pipeline <- reader_las(filter = keep_ground()) + del + dtm
  ans = exec(pipeline, on = olas)

  expect_true(all(as.numeric(na.omit(as.numeric(ans[]))) == 0))

  del = triangulate(use_attribute = "PP")
  dtm = rasterize(2, del)
  pipeline <- reader_las(filter = keep_ground()) + del + dtm
  expect_error(exec(pipeline, on = olas), "no extrabyte attribute 'PP' found")
})

test_that("tw can translate with a matrix ",
{
  m = matrix(c(1,0,0,-500000,
               0,1,0,-500000,
               0,0,1,150,
               0,0,0,1), 4, 4, byrow = TRUE)

  f <- system.file("extdata", "Megaplot.las", package="lasR")

  olas = templas()
  pipeline <- reader_las() + transform_with(m) + summarise() + write_las(olas)
  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 81590)
})

test_that("tw can rotate with a matrix ",
{
  a = pi/4
  m = matrix(c(cos(a),-sin(a),0,0,
               sin(a),cos(a),0,0,
               0,0,1,100,
               0,0,0,1), 4, 4, byrow = TRUE)

  f <- system.file("extdata", "Megaplot.las", package="lasR")

  olas = templas()
  pipeline <- reader_las() + transform_with(m) + summarise() + write_las(olas)
  ans = exec(pipeline, on = f)

  las = read_las(ans$write_las)
  xrange = range(las$X)
  yrange = range(las$Y)
  zrange = range(las$Z)

  expect_equal(ans$summary$npoints, 81590)
  expect_equal(xrange, c(-3064063.75, -3063738.72))
  expect_equal(yrange, c(4032305.31, 4032629.32))
  expect_equal(zrange, c(100.00, 129.97))
})

test_that("tw fails with non orthogonal matrix",
{
  f <- system.file("extdata", "Example.las", package="lasR")

  m = matrix(c(8,0,0,-500000,
               0,1,0,-500000,
               0,0,1,150,
               0,0,0,1), 4, 4, byrow = TRUE)

  pipeline <- transform_with(m)
  expect_error(exec(pipeline, on = f), "the matrix is not orthogonal")

  m = matrix(c(1,0,1,-500000,
               0,1,0,-500000,
               0,0,1,150,
               0,0,0,1), 4, 4, byrow = TRUE)

  pipeline <- transform_with(m)
  expect_error(exec(pipeline, on = f), "the matrix is not orthogonal")
})
test_that("tw can normalize",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  mesh  = triangulate(50)
  norm = mesh + transform_with(mesh)
  pipeline = reader_las(filter = "-keep_class 2") + norm + write_las()
  g = exec(pipeline, on = f)

  pipeline = reader_las(filter = "-keep_class 2") + summarise()
  u = exec(pipeline, on = g)

  expect_equal(as.numeric(names(u$z_histogram)), c(0))
  expect_equal(unname(u$z_histogram), c(8159))
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
  expect_equal(sd(u), 0.075, tolerance = 0.003)
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

  #del = triangulate(use_attribute = "PP")
  #dtm = rasterize(2, del)
  #pipeline <- reader_las(filter = keep_ground()) + del + dtm
  #expect_error(exec(pipeline, on = olas), "no extrabyte attribute 'PP' found")
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

test_that("tw with operator '=' stores raster value as-is into a per-point attribute",
{
  f = system.file("extdata", "MixedConifer.las", package="lasR")

  reader = reader_las(filter = "-keep_first")
  chm = rasterize(1, "max")
  lmx = local_maximum(5)
  tree = region_growing(chm, lmx, max_cr = 10)
  exby = add_extrabytes("int", "MYTID", "tree segmentation ID")
  # Use nearest-neighbor sampling so the integer label is preserved exactly.
  set  = transform_with(tree, operator = "=", store_in_attribute = "MYTID", bilinear = FALSE)

  olas = templas()
  pipeline = reader + chm + lmx + tree + exby + set + write_las(olas)
  ans = exec(pipeline, on = f)

  read = reader_las() + callback(function(data) { return(data$MYTID) }, expose = "E")
  ids  = exec(read, on = ans$write_las)

  expect_true(any(ids > 0))                  # at least some points were labelled
  expect_true(any(ids == 0))                 # unsegmented points kept their default 0 (not deleted)

  # With nearest-neighbor sampling the per-point IDs must be a subset of the raster IDs.
  raster_ids = sort(unique(na.omit(as.numeric(ans$region_growing[]))))
  point_ids  = sort(unique(ids[ids > 0]))
  expect_true(all(point_ids %in% raster_ids))
})

test_that("tw operator '=' requires store_in_attribute",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  mesh = triangulate(50, filter = keep_ground())
  dtm  = rasterize(1, mesh, ofile = "")
  bad  = transform_with(dtm, operator = "=")
  pipeline = reader_las() + mesh + dtm + bad

  expect_error(exec(pipeline, on = f), "store_in_attribute")
})

test_that("tw rejects an unsupported operator",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  mesh = triangulate(50, filter = keep_ground())
  dtm  = rasterize(1, mesh, ofile = "")
  bad  = transform_with(dtm, operator = "*")
  pipeline = reader_las() + mesh + dtm + bad

  expect_error(exec(pipeline, on = f), "invalid operator")
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
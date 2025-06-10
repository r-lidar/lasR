f = paste0(system.file(package="lasR"), "/extdata/bcts/")
f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

read = function()
{
  load = function(data) { return(data) }
  call = callback(load, expose = "xyzrib", no_las_update = TRUE)
  call
}

test_that("reader_circle perform a queries",
{
  pipeline = reader_las_circles(885100, 629300, 10, filter = keep_first()) + read()
  ans = exec(pipeline, f)
  expect_equal(dim(ans), c(2665L, 6L))
  expect_equal(table(ans$ReturnNumber), c(`1` = 2665L), ignore_attr = T)

  # between two tiles
  pipeline = reader_las_circles(885150, 629400, 10, filter = keep_ground()) + read()
  ans = exec(pipeline, f)
  expect_equal(dim(ans), c(314L, 6L))

  # between two tiles with a buffer
  pipeline = reader_las_circles(885150, 629400, 10) + read()
  ans = exec(pipeline, f, buffer = 5)

  expect_equal(dim(ans), c(14074, 6L))
  expect_equal(sum(ans$Buffer), 7706L)

  # between two tiles with a buffer
  pipeline = reader_las_circles(885150, 629400, 10, filter = keep_first()) + read()
  ans = exec(pipeline, f, buffer = 5)

  expect_equal(dim(ans), c(10263, 6L))
  expect_equal(sum(ans$Buffer), 5691L)

  # between two tiles with a buffer but the centroid is not in a file
  pipeline = reader_las_rectangles(885000L, 629390, 885040, 629410) + read()
  ans = exec(pipeline, f, buffer = 5)
  expect_equal(dim(ans), c(5028L, 6L))
  expect_equal(sum(ans$Buffer), 2415L)

  # no match
  pipeline = reader_las_circles(8850000, 629400, 20) + read()
  expect_error(exec(pipeline, f, buffer = 5), "cannot find")
})

test_that("reader_circle creates raster with minimal bbox",
{
  pipeline = reader_las_circles(c(885150, 885150), c(629300, 629600) , 10, filter = keep_ground()) + rasterize(2, "min")
  ans = exec(pipeline, on = f)

  expect_equal(dim(ans), c(161, 11, 1))
  expect_equal(sum(is.na(ans[])), 1622)
})

test_that("circle buffer is removed #80",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")
  ans <- exec(reader_las_circles(273500, 5274500, 20) + rasterize(2, "z_mean"), on = f, buffer = 20)

  expect_equal(sum(is.na(ans[])), 121)
  expect_equal(terra::xmin(ans), 273480)
  expect_equal(terra::xmax(ans), 273522)
  expect_equal(terra::ymin(ans), 5274480)
  expect_equal(terra::ymax(ans), 5274522)
})


test_that("reader_circle works with a buffer (#141)",
{
  file <- c(system.file("extdata", "MixedConifer.las", package="lasR"))

  read <- lasR::reader_circles(xc = 481305, yc = 3812966, r = 50)
  pipeline = read + summarise() + lasR::write_las(ofile = paste0(tempfile(), ".las"))

  res1 <- lasR::exec(pipeline, on = file, buffer = 0)
  res2 <- lasR::exec(pipeline, on = file, buffer = 30)

  res1 = read_las(res1$write_las)
  res2 = read_las(res2$write_las)

  expect_equal(res1, res2)
  expect_equal(dim(res1), c(29488, 18))
})

test_that("circle buffer is removed #142",
{
  file <- c(system.file("extdata", "Megaplot.las", package="lasR"))

  read_small <- reader_circles(xc = 684876.6, yc = 5017902, r = 10)
  chm <- lasR::chm(res = 1)

  ans <- lasR::exec(read_small + chm, on = file)

  expect_equal(sum(is.na(ans[])), 157)
  expect_equal(terra::xmin(ans), 684876 - 10)
  expect_equal(terra::xmax(ans), 684876 + 11)
  expect_equal(terra::ymin(ans), 5017902 - 10)
  expect_equal(terra::ymax(ans), 5017902 + 11)

  ans <- lasR::exec(read_small + chm, on = file, buffer = 10)

  expect_equal(sum(is.na(ans[])), 146)
  expect_equal(terra::xmin(ans), 684876 - 10)
  expect_equal(terra::xmax(ans), 684876 + 11)
  expect_equal(terra::ymin(ans), 5017902 - 10)
  expect_equal(terra::ymax(ans), 5017902 + 11)

  read_large <- lasR::reader_circles(xc = 684876.6, yc = 5017902, r = 80)

  ans <- lasR::exec(read_large + chm, on = file)

  expect_equal(sum(is.na(ans[])), 7756)
  expect_equal(terra::xmin(ans), 684876 - 80)
  expect_equal(terra::xmax(ans), 684876 + 81)
  expect_equal(terra::ymin(ans), 5017902 - 80)
  expect_equal(terra::ymax(ans), 5017902 + 81)
})

test_that("circle buffer is removed #143",
{
  read_small <- reader_circles(xc = 684876.6, yc = 5017902, r = 800)
  chm <- lasR::chm(res = 1)

  #ans <- lasR::exec(read_small + chm, on = file)

  # TO BE TESTED DOES NOT WORK YET
})




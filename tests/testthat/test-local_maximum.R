coords_sorted = function(x)
{
  coords = sf::st_coordinates(x)
  coords = coords[do.call(order, as.data.frame(coords)), , drop = FALSE]
  rownames(coords) = NULL
  coords
}

test_that(".build_ws_lut handles scalar, function, and bad input",
{
  expect_null(lasR:::.build_ws_lut(3))

  lut <- lasR:::.build_ws_lut(function(x) 0.1 * x + 2)
  expect_type(lut, "list")
  expect_equal(lut$min, 0)
  expect_equal(lut$step, 0.5)
  expect_equal(length(lut$lut), length(seq(0, 200, 0.5)))
  expect_equal(lut$lut[1], 2)
  expect_equal(lut$lut[length(lut$lut)], 0.1 * 200 + 2)

  flat <- lasR:::.build_ws_lut(function(x) 3)
  expect_true(all(flat$lut == 3))

  expect_error(lasR:::.build_ws_lut("foo"), "number or a function")
  expect_error(lasR:::.build_ws_lut(c(1, 2)), "single number")
  expect_error(lasR:::.build_ws_lut(-1), "positive")
  expect_error(
    lasR:::.build_ws_lut(function(x) rep(NA_real_, length(x))), "NA")
  expect_error(
    lasR:::.build_ws_lut(function(x) -x - 1), "negative or null")
  expect_error(
    lasR:::.build_ws_lut(function(x) c(1, 2)), "correct output")
})

test_that("local maximum works",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  lmf = local_maximum(3, record_attributes = T)
  ans = exec(lmf, on = f)

  expect_equal(dim(ans), c(297, 6))

  lmf = local_maximum(3)
  ans = exec(lmf, on = f)

  expect_equal(dim(ans), c(297, 1))
})

test_that("local maximum works with wildcard (#62)",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  lmf = local_maximum(3, record_attributes = T, ofile = paste0(tempdir(), "/*_ttops.gpkg"))
  ans = exec(lmf, on = f)

  expect_equal(dim(ans), c(297, 6))
})

test_that("local maximum works with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  lmf = local_maximum(5, min_height = 330, record_attributes = T)
  ans = exec(lmf, on = f)

  expect_equal(dim(ans), c(2234, 6))
})

test_that("local maximum works with extra bytes",
{
  f <- system.file("extdata", "extra_byte.las", package="lasR")

  lmf = local_maximum(3, use_attribute = "Pulse width", record_attributes = T)
  ans = suppressWarnings(exec(lmf, on = f))

  z = sf::st_coordinates(ans)[,3]
  #expect_equal(range(z), c(5.1, 8.4))
  expect_equal(length(z), 7L)
})

test_that("local maximum records use_attribute as a vector field",
{
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")

  tri    <- triangulate(filter = keep_ground_and_water())
  hag_eb <- add_extrabytes("double", "HAG", "Height Above Ground")
  hag    <- transform_with(tri, store_in_attribute = "HAG")
  lmf    <- local_maximum(
    ws = 3,
    min_height = 2,
    filter = "HAG > 2",
    use_attribute = "HAG",
    record_attributes = TRUE
  )

  ans <- exec(reader() + tri + hag_eb + hag + lmf, on = f)

  expect_true("HAG" %in% names(ans))
  expect_type(ans$HAG, "double")
  expect_true(all(ans$HAG > 2))
})

#test_that("local maximum fails with missing extra bytes",
#{
#  f <- system.file("extdata", "extra_byte.las", package="lasR")
#
#  lmf = local_maximum(3, use_attribute = "Foo")

#  expect_error(suppressWarnings(exec(lmf, on = f)), "no extrabyte attribute 'Foo' found")
#})

test_that("local maximum works with a raster",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")

  chm = chm()
  pit = pit_fill(chm)
  lmf1 = local_maximum_raster(chm, 4)
  lmf2 = local_maximum_raster(pit, 4)

  ans = exec(chm  + lmf1 + pit + lmf2, on = f)

  expect_equal(dim(ans[[2]]), c(258L, 1L))
  expect_equal(dim(ans[[4]]), c(220L, 1L))
})

test_that("local maximum works with a raster with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  reader = reader_las(filter = keep_first())
  chm = rasterize(1, "max")
  lmx = local_maximum_raster(chm, 5, min_height = 330)

  u  = exec(chm + lmx, on = f)

  #terra::plot(u$rasterize, col = lidR::height.colors(25))
  #plot(u$local_maximum$geom, add = TRUE, cex = 0.1, pch = 19)

  expect_equal(nrow(u$local_maximum), 2099L)
})

test_that("local maximum can flag the points",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")

  # Store in 'UserData'
  lmf <- local_maximum(5, ofile = "", store_in_attribute = "UserData")
  ans <- exec(lmf + write_las(), on = f)
  ans
  res = read_las(ans)
  expect_equal(sum(res$UserData), 177)

  # Store in 'lm' but this attribute does not exist
  lmf <- local_maximum(5, ofile = "", store_in_attribute = "lm")
  expect_error(exec(lmf + write_las(), on = f))

  # Store in 'lm' but add it first
  attr <- add_extrabytes("uchar", "lm", "local maximum flag")
  lmf <- local_maximum(5, ofile = "", store_in_attribute = "lm")
  ans <- exec(attr + lmf + write_las(), on = f)
  res = read_las(ans)
  expect_equal(sum(res$lm), 177)
})


test_that("local maximum works with deleted points",
{
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  del = delete_points("Z < 0.1")
  lmf <- local_maximum(5)
  pipeline <- del + lmf
  ans <- exec(pipeline, on = f, ncores = 1)

  expect_equal(nrow(ans), 177)
})

test_that("variable window with a constant function equals scalar ws",
{
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")

  a <- exec(local_maximum(3), on = f)
  b <- exec(local_maximum(function(x) 3), on = f)

  expect_equal(coords_sorted(a), coords_sorted(b))
})

test_that("variable window sets the tile buffer from the widest window",
{
  # need_buffer() must report the widest possible window so tile/edge
  # handling stays correct. max over 0-200 m of 0.1*x + 2 is 22; a
  # scalar ws keeps reporting itself.
  vw <- local_maximum(function(x) 0.1 * x + 2)
  expect_equal(lasR:::get_pipeline_info(vw)$buffer, 22)
  expect_equal(lasR:::get_pipeline_info(local_maximum(3))$buffer, 3)
})

test_that("variable window function detects tree tops",
{
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")

  lmf <- local_maximum(ws = function(x) 0.1 * x + 2)
  ans <- exec(lmf, on = f)

  expect_s3_class(ans, "sf")
  expect_gt(nrow(ans), 0)

  small <- exec(local_maximum(function(x) 0.1 * x + 2), on = f)
  large <- exec(local_maximum(function(x) 0.1 * x + 6), on = f)
  expect_lte(nrow(large), nrow(small))
})

test_that("variable window roughly matches lidR lmf",
{
  skip_if_not_installed("lidR")
  skip_if_not_installed("sf")

  f  <- system.file("extdata", "MixedConifer.las", package = "lasR")
  fn <- function(x) 0.1 * x + 2

  las   <- lidR::readLAS(f)
  ttops <- lidR::locate_trees(las, lidR::lmf(ws = fn, hmin = 2))
  ours  <- exec(local_maximum(ws = fn, min_height = 2), on = f)

  expect_lt(abs(nrow(ours) - nrow(ttops)) / nrow(ttops), 0.10)
})

test_that("variable window works on a raster",
{
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")

  chm  <- chm()
  flat <- local_maximum_raster(chm, 4)
  func <- local_maximum_raster(chm, function(x) 4)

  ans <- exec(chm + flat + func, on = f)

  expect_equal(coords_sorted(ans[[2]]), coords_sorted(ans[[3]]))
})


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


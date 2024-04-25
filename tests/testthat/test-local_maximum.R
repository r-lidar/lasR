test_that("local maximum works",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  read = reader(f)
  lmf = local_maximum(3, record_attributes = T)
  ans = processor(read + lmf)

  expect_equal(dim(ans), c(297, 6))

  read = reader(f)
  lmf = local_maximum(3)
  ans = processor(read + lmf)

  expect_equal(dim(ans), c(297, 1))
})

test_that("local maximum works with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  read = reader(f)
  lmf = local_maximum(5, min_height = 330, record_attributes = T)
  ans = processor(read + lmf)

  expect_equal(dim(ans), c(2235, 6))
})

test_that("local maximum works with extra bytes",
{
  f <- system.file("extdata", "extra_byte.las", package="lasR")

  read = reader(f)
  lmf = local_maximum(3, use_attribute = "Pulse width", record_attributes = T)
  ans = suppressWarnings(processor(read + lmf))

  z = sf::st_coordinates(ans)[,3]
  expect_equal(range(z), c(5.1, 8.4))
  expect_equal(length(z), 7L)
})

test_that("local maximum fails with missing extra bytes",
{
  f <- system.file("extdata", "extra_byte.las", package="lasR")

  read = reader(f)
  lmf = local_maximum(3, use_attribute = "Foo")

  expect_error(suppressWarnings(processor(read + lmf)), "no extrabyte attribute 'Foo' found")
})

test_that("local maximum works with a raster",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")

  chm = chm()
  pit = pit_fill(chm)
  lmf1 = local_maximum_raster(chm, 4)
  lmf2 = local_maximum_raster(pit, 4)

  ans = exec(chm  + lmf1 + pit + lmf2, on = f)

  expect_equal(dim(ans[[2]]), c(262L, 1L))
  expect_equal(dim(ans[[4]]), c(222L, 1L))
})

test_that("growing region works with a raster with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  reader = reader(f, filter = keep_first())
  chm = rasterize(1, "max")
  lmx = local_maximum_raster(chm, 5, min_height = 330)

  u  = processor(reader + chm + lmx)

  #terra::plot(u$rasterize, col = lidR::height.colors(25))
  #plot(u$local_maximum$geom, add = TRUE, cex = 0.1, pch = 19)

  expect_equal(nrow(u$local_maximum), 2111L)
})


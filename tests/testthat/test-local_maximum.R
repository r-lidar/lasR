test_that("local maximum works",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  read = reader(f)
  lmf = local_maximum(3)
  ans = processor(read + lmf)

  expect_equal(dim(ans), c(297, 6))
})

test_that("local maximum works with multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  read = reader(f)
  lmf = local_maximum(5)
  ans = processor(read + lmf)

  expect_equal(dim(ans), c(2412L, 6)) # There is a data race !!
})

test_that("local maximum works with extra bytes",
{
  f <- system.file("extdata", "extra_byte.las", package="lasR")

  read = reader(f)
  lmf = local_maximum(3, use_attribute = "Pulse width")
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

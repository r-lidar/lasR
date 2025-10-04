test_that("write lax works on-the-fly",
{
  tmpdir = paste0(tempdir(), "/testlax")
  if (!dir.exists(tmpdir)) dir.create(tmpdir)

  sink(tempfile())

  # create a datset without lax
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  z = file.copy(f, tmpdir)
  f = list.files(tmpdir, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, c(F,F,F,F))

  # between two tiles with a buffer
  # write lax if > 1 query
  pipeline = reader_las_circles(c(885150, 885150), c(629400, 629400), 10, buffer = 5L) + lasR:::nothing()
  ans = exec(pipeline, on = f)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, c(T,T,F,F))

  lax = list.files(tmpdir, pattern = "(?i)\\.lax$", full.names = TRUE)
  expect_length(lax, 2L)

  # between two tiles no buffer
  # write lax if > 1 query
  pipeline = reader_las_circles(c(885150,885150), c(630100,630100), 10) + lasR:::nothing()
  ans = exec(pipeline, on = f)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, c(T,T,F,T))

  unlink(tmpdir, recursive = T)

  sink(NULL)
})

test_that("write lax works before anything else (not on-the-fly)",
{
  tmpdir = paste0(tempdir(), "/testlax2")
  if (!dir.exists(tmpdir)) dir.create(tmpdir)

  sink(tempfile())

  # create a datset without lax
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  z = file.copy(f, tmpdir)
  f = list.files(tmpdir, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, c(F,F,F,F))

  # between two tiles with a buffer
  vpc = tempfile(fileext = ".vpc")
  pipeline = reader_las_circles(885150, 629400, 10, buffer = 5L) + lasR:::nothing() + write_lax(TRUE) + write_vpc(ofile = vpc)
  ans = exec(pipeline, on = f, progress = TRUE)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, c(T,T,T,T))

  lax = list.files(tmpdir, pattern = "(?i)\\.lax$", full.names = TRUE)
  expect_length(lax, 0L)

  ans = sf::st_read(vpc, quiet = TRUE)
  expect_true(all(ans$pc.indexed == "true"))

  unlink(tmpdir, recursive = T)

  sink(NULL)
})


test_that("write lax works by chunk",
{
  tmpdir = paste0(tempdir(), "/testlax")
  if (!dir.exists(tmpdir)) dir.create(tmpdir)

  sink(tempfile())

  # create a datset without lax
  f <- system.file("extdata", "Topography.las", package = "lasR")
  z = file.copy(f, tmpdir)
  f = list.files(tmpdir, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, FALSE)

  # between two tiles with a buffer
  pipeline = lasR:::nothing(TRUE)
  ans = exec(pipeline, on = f, chunk = 100)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, T)

  unlink(tmpdir, recursive = T)

  sink(NULL)
})

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
  pipeline = reader_circles(f, 885150, 629400, 10, buffer = 5L) + lasR:::nothing()
  ans = processor(pipeline)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, c(T,T,F,F))

  lax = list.files(tempdir(), pattern = "(?i)\\.lax$", full.names = TRUE)
  expect_length(lax, 0L)

  # between two tiles no buffer
  pipeline = reader_circles(f, 885150, 630100, 10) + lasR:::nothing()
  ans = processor(pipeline)

  indexed = lasR:::is_indexed(f)
  expect_equal(indexed, c(T,T,F,T))

  file.remove(f)

  sink(NULL)
})

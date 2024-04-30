test_that("writelas works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = paste0(tempdir(), "/test.las")
  reader = reader_las(filter = "-keep_first")
  writer = write_las(o)
  u = exec(reader + writer, on = f)

  expect_type(u, "character")
  expect_true(all(file.exists(u)))
  expect_length(u, 1)

  v = exec(summarise(), on = o)

  expect_equal(v$npoints, 53538)
  expect_equal(v$npoints_per_return, c("1" = 53538L))
  expect_equal(v$npoints_per_class, c("1" = 44151L, "2" = 5490L, "9" = 3897L))
})

test_that("writelas writes 4 files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  reader = reader_las(filter = "-keep_first")
  writer = write_las()
  u = exec(reader + writer, on = f)

  f1 = sub(pattern = "(.*)\\..*$", replacement = "\\1", basename(f))
  f1 = paste0(f1, ".las")

  expect_type(u, "character")
  expect_length(u, 4)
  expect_equal(basename(u), f1)
  expect_true(all(file.exists(u)))
})

test_that("writelas writes 1 merged file",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  o = paste0(tempdir(), "/test.las")

  reader = reader_las(filter = "-keep_first")
  writer = write_las(o)
  u = exec(reader + writer, on = f)

  expect_type(u, "character")
  expect_length(u, 1)
  expect_true(all(file.exists(u)))

  v = exec(reader_las() + summarise(), on = o)

  expect_equal(v$npoints, 1981696L)
  expect_equal(v$npoints_per_return, c("1" = 1981696L))
  expect_equal(v$npoints_per_class, c("1" = 1899830L, "2" = 81866L))
})

test_that("summary works with 1 file",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f)
  summ = summarise()
  pipeline = read + summ
  u = processor(pipeline)

  expect_type(u, "list")
  expect_equal(u$npoints, 30)
  expect_equal(u$npoints_per_return, c("1" = 26L, "2" = 4L))
  expect_equal(u$npoints_per_class, c( "1" = 27L, "2" = 3L))
})

test_that("summary works with 4 files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader(f)
  summ = summarise()
  pipeline = read + summ
  u = processor(pipeline)

  expect_type(u, "list")
  expect_equal(u$npoints, 2834350)
  expect_equal(u$npoints_per_return, c(1981696L, 746460L, 101739L, 4455L), ignore_attr = TRUE)
  expect_equal(u$npoints_per_class, c(2684009L, 150341L), ignore_attr = TRUE)
})

test_that("summary works one non streaming mode with buffer",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader(f, buffer = 50)
  summ = summarise()
  pipeline = read + summ
  u = processor(pipeline)

  expect_type(u, "list")
  expect_equal(u$npoints, 2834350)
  expect_equal(u$npoints_per_return, c(1981696L, 746460L, 101739L, 4455L), ignore_attr = TRUE)
  expect_equal(u$npoints_per_class, c(2684009L, 150341L), ignore_attr = TRUE)
})

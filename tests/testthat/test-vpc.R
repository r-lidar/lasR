tmpdir = paste0(tempdir(), "/testvpc")
if (!dir.exists(tmpdir)) dir.create(tmpdir)

# create a datset without lax
f = paste0(system.file(package="lasR"), "/extdata/bcts/")
f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
z = file.copy(f, tmpdir)
f = list.files(tmpdir, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

test_that("vpc reader works",
{
  f = system.file("extdata", "bcts/bcts.vpc", package="lasR")
  pipeline = reader(f) + hulls()
  ans = processor(pipeline)

  expect_s3_class(ans, "sf")
  expect_equal(dim(ans), c(4L,1L))
  expect_equal(length(ans$geom[[1]]), 1L)
})

test_that("vpc writer works",
{
  o = tempfile(fileext = ".vpc")
  pipeline = reader(f) + write_vpc(o)
  ans = processor(pipeline)

  expect_true(file.exists(ans))

  pipeline = reader(ans) + hulls()
  ans = processor(pipeline)

  expect_s3_class(ans, "sf")
  expect_equal(dim(ans), c(4L,1L))
  expect_equal(length(ans$geom[[1]]), 1L)
})

file.remove(f)

test_that("vpc reader fail",
{
  f = tempfile(fileext = ".vpc")
  file.create(f)
  pipeline = lasR:::nothing()
  expect_error(exec(pipeline, on = f), "JSON parsing error")
})

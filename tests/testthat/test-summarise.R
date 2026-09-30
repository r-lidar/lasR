test_that("summary works with 1 file",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader_las()
  summ = summarise()
  pipeline = read + summ
  u = exec(pipeline, on = f)

  expect_type(u, "list")
  expect_equal(u$npoints, 30)
  expect_equal(u$npoints_per_return, c("1" = 26L, "2" = 4L))
  expect_equal(u$npoints_per_class, c( "1" = 27L, "2" = 3L))
  expect_equal(u$epsg, 26917)
})

test_that("summary preserves metric order",
{
  skip("no implemented")

  f <- system.file("extdata", "Example.las", package="lasR")
  p = summarise(metrics = c("z_max", "i_min", "r_mean", "n_median", "c_sd", "t_cv", "u_sum", "p_mode", "a_mean", "count", "z_p95", "R_sum", "B_mean", "z_above975"))
  ans = exec(p, on = f, noread = T)
  m = ans$metrics

  expect_equal(names(m), c("z_max", "i_min", "r_mean", "n_median", "c_sd", "t_cv", "u_sum", "p_mode", "a_mean", "count", "z_p95", "R_sum", "B_mean", "z_above975"))
})

test_that("summary works with 4 files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader_las()
  summ = summarise()
  pipeline = read + summ
  u = exec(pipeline, on = f)

  expect_type(u, "list")
  expect_equal(u$npoints, 2834350)
  expect_equal(u$npoints_per_return, c(1981696L, 746460L, 101739L, 4455L), ignore_attr = TRUE)
  expect_equal(u$npoints_per_class, c(2684009L, 150341L), ignore_attr = TRUE)
})

test_that("summary works on non streaming mode with buffer",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader_las()
  summ = summarise()
  pipeline = read + summ
  u = exec(pipeline, on = f, buffer = 50)

  expect_type(u, "list")
  expect_equal(u$npoints, 2834350)
  expect_equal(u$npoints_per_return, c(1981696L, 746460L, 101739L, 4455L), ignore_attr = TRUE)
  expect_equal(u$npoints_per_class, c(2684009L, 150341L), ignore_attr = TRUE)
})

test_that("summary can compute metrics on multiple file",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader_las_circles(c(885100, 885100, 885100), c(629400, 629600, 629800), 11.28)
  metrics = summarise(metrics = c("z_mean", "z_p95", "i_median", "count"))
  pipeline = read + metrics

  info = lasR:::get_pipeline_info(pipeline)
  expect_false(info$streamable)

  u = exec(pipeline, on = f)

  expect_equal(dim(u$metrics), c(3, 4))
  expect_equal(u$npoints, sum(u$metrics$count))
})

test_that("summary reports area, point density and pulse density",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader_las()
  summ = summarise()
  pipeline = read + summ
  u = exec(pipeline, on = f)

  expect_false(is.null(u$area))
  expect_false(is.null(u$density))
  expect_false(is.null(u$pulse_density))

  expect_gt(u$area, 0)
  expect_equal(u$density, u$npoints / u$area)
  expect_equal(u$pulse_density, unname(u$npoints_per_return["1"]) / u$area)
  expect_gt(u$density, 0)
  expect_gt(u$pulse_density, 0)
})

test_that("summary pulse density uses first returns on multiple files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  read = reader_las()
  summ = summarise()
  pipeline = read + summ
  u = exec(pipeline, on = f)

  expect_gt(u$area, 0)
  expect_equal(u$density, 2834350 / u$area)
  expect_equal(u$pulse_density, 1981696 / u$area)
})

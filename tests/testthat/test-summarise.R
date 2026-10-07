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

test_that("summary area is the disc area for a circular query",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  xc = 273400
  yc = 5274450
  r = 11.28

  pipeline = reader_circles(xc, yc, r) + summarise()
  u = exec(pipeline, on = f)

  las = read_las(f)
  inside = (las$X - xc)^2 + (las$Y - yc)^2 <= r^2
  n = sum(inside)
  nfirst = sum(inside & las$ReturnNumber == 1L)

  expect_equal(u$npoints, n)
  expect_equal(u$area, pi * r^2)
  expect_equal(u$density, n / (pi * r^2))
  expect_equal(u$pulse_density, nfirst / (pi * r^2))
})

test_that("summary area sums the disc areas of multiple circular queries",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  xc = c(273400, 273500)
  yc = c(5274450, 5274550)
  r = c(11.28, 15)

  las = read_las(f)
  inside = (las$X - xc[1])^2 + (las$Y - yc[1])^2 <= r[1]^2 | (las$X - xc[2])^2 + (las$Y - yc[2])^2 <= r[2]^2
  n = sum(inside)

  # Streaming mode and non streaming mode (metrics) must report the same area
  for (metrics in list(NULL, "count"))
  {
    pipeline = reader_circles(xc, yc, r) + summarise(metrics = metrics)
    u = exec(pipeline, on = f)

    expect_equal(u$npoints, n)
    expect_equal(u$area, sum(pi * r^2))
    expect_equal(u$density, n / sum(pi * r^2))
  }
})

test_that("summary area is the rectangle area for a rectangular query",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  las = read_las(f)
  n = sum(las$X >= 273400 & las$X <= 273450 & las$Y >= 5274450 & las$Y <= 5274520)

  pipeline = reader_rectangles(273400, 5274450, 273450, 5274520) + summarise()

  # The buffer is excluded from the area
  for (buffer in c(0, 20))
  {
    u = exec(pipeline, on = f, buffer = buffer)

    expect_equal(u$npoints, n)
    expect_equal(u$area, 50 * 70)
    expect_equal(u$density, n / (50 * 70))
  }
})

test_that("summary area ignores queries outside the coverage but within the buffer",
{
  f = system.file("extdata", "Topography.las", package="lasR")

  # The second query lies east of the file (xmax = 273642.9) but within the
  # buffer, so its chunk extent, once clipped to the coverage, is inverted.
  read = reader_rectangles(c(273400, 273650), c(5274450, 5274450), c(273450, 273660), c(5274520, 5274460))
  u = exec(read + summarise(), on = f, buffer = 20)
  expect_equal(u$area, 50 * 70)
  expect_equal(u$density, u$npoints / (50 * 70))

  read = reader_circles(c(273400, 273660), c(5274450, 5274450), 5)
  u = exec(read + summarise(), on = f, buffer = 20)
  expect_equal(u$area, pi * 5^2)
  expect_equal(u$density, u$npoints / (pi * 5^2))
})

test_that("summary area ignores circles north or south of the coverage but within the buffer",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  las = read_las(f)
  ymin = min(las$Y)
  ymax = max(las$Y)

  # The second circle lies wholly north (then south) of the file but within the
  # buffer: its chunk extent, once clipped to the coverage, is inverted in y only.
  # It selects no point and must not add a disc to the area.
  for (yc in c(ymax + 8, ymin - 8))
  {
    read = reader_circles(c(273400, 273500), c(5274450, yc), c(11.28, 5))
    u = exec(read + summarise(), on = f, buffer = 20)

    n = sum((las$X - 273400)^2 + (las$Y - 5274450)^2 <= 11.28^2)
    expect_equal(u$npoints, n)
    expect_equal(u$area, pi * 11.28^2)
    expect_equal(u$density, n / (pi * 11.28^2))
  }
})

test_that("summary area of a circle crossing the coverage edge is the area of the selected region",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  las = read_las(f)
  ext = c(min(las$X), min(las$Y), max(las$X), max(las$Y))
  r = 11.28

  # A circular query crossing the edge of the coverage is clipped to the
  # coverage. The points selected are those of the clipped extent that lie
  # within the disc of radius half its width centred on it. Compute that region,
  # its area (numerical integration) and its point count independently.
  selected_region = function(xc, yc)
  {
    bx = c(max(xc - r, ext[1]), min(xc + r, ext[3]))
    by = c(max(yc - r, ext[2]), min(yc + r, ext[4]))
    rr = diff(bx) / 2
    cx = mean(bx)
    cy = mean(by)
    a = min(diff(by) / 2, rr)
    area = stats::integrate(function(y) 2 * sqrt(pmax(rr^2 - y^2, 0)), -a, a, rel.tol = 1e-12)$value
    keep = las$X >= bx[1] & las$X <= bx[2] & las$Y >= by[1] & las$Y <= by[2] & (las$X - cx)^2 + (las$Y - cy)^2 <= rr^2
    list(area = area, n = sum(keep), full = pi * r^2)
  }

  # North and south edges (clipped in y), inside and outside the edge, and the
  # east edge (clipped in x)
  xc = c(273500, 273500, 273500, 273500, ext[3] - 3)
  yc = c(ext[4] - 3, ext[4] + 5, ext[4] + 9, ext[2] + 5, 5274500)

  for (i in seq_along(xc))
  {
    expected = selected_region(xc[i], yc[i])
    u = exec(reader_circles(xc[i], yc[i], r) + summarise(), on = f)

    expect_lt(expected$area, expected$full)
    expect_equal(u$npoints, expected$n)
    expect_equal(u$area, expected$area)
    expect_equal(u$density, expected$n / expected$area)
  }
})

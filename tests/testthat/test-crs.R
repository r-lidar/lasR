read_crs = function(f) { exec(summarise(), on = f)$crs }

test_that("Object are assigned a CRS",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  pipeline <- rasterize(5, "count") + local_maximum(5)
  ans = exec(pipeline, on = f)

  expect_match(terra::crs(ans[[1]]), "NAD83\\(CSRS\\) / MTM zone 7")
  expect_match(sf::st_crs(ans[[2]])$input, "NAD83\\(CSRS\\) / MTM zone 7")
})

test_that("set_crs works with epsg",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  pipeline <- reader_las(filter = "-keep_random_fraction 0.1") + rasterize(20, "count") + set_crs(2044) + local_maximum(5)  + set_crs(2004) + write_las()
  ans = exec(pipeline, on = f)

  las_crs = read_crs(ans$write_las)

  expect_match(terra::crs(ans[[1]]), "NAD83\\(CSRS\\) / MTM zone 7")
  expect_match(sf::st_crs(ans[[2]])$input, "Hanoi 1972 / Gauss-Kruger zone 18")
  expect_match(las_crs, "Montserrat 1958 / British West Indies Grid")
})

test_that("set_crs works with wkt",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  wkt1 = sf::st_crs(2044)$wkt
  wkt2 = sf::st_crs(2004)$wkt

  pipeline <- reader_las(filter = "-keep_random_fraction 0.1") + rasterize(20, "count") + set_crs(wkt1) + local_maximum(5)  + set_crs(wkt2) + write_las()
  ans = exec(pipeline, on = f)

  las_crs = read_crs(ans$write_las)

  expect_match(terra::crs(ans[[1]]), "NAD83\\(CSRS\\) / MTM zone 7")
  expect_match(sf::st_crs(ans[[2]])$input, "Hanoi 1972 / Gauss-Kruger zone 18")
  expect_match(las_crs, "Montserrat 1958 / British West Indies Grid")
})

test_that("set_crs fails with invalid epsg",
{
  f <- system.file("extdata", "Example.las", package="lasR")

  pipeline <- set_crs(12) + rasterize(20, "count") + write_las()
  expect_error(exec(pipeline, on = f), "crs not found")
})

test_that("set_crs fails with invalid wkt",
{
  f <- system.file("extdata", "Example.las", package="lasR")

  pipeline <- set_crs("BLA") + rasterize(20, "count") + write_las()
  expect_error(exec(pipeline, on = f), "WKT string")
})

test_that("an invalid CRS does not silence the GDAL error messages",
{
  # The CRS constructors push a quiet GDAL error handler. It was not popped when the CRS is
  # invalid, so the GDAL errors reported afterwards to R (by the handler of terra or sf) were
  # silently discarded.
  skip_if_not(requireNamespace("terra", quietly = TRUE) || requireNamespace("sf", quietly = TRUE))

  f <- system.file("extdata", "Example.las", package="lasR")

  # A GDAL error raised by lasR: the raster cannot be created in a directory that does not exist
  gdal_errors <- function()
  {
    tif <- file.path(tempdir(), "nonexistent", "x.tif")
    n <- 0
    withCallingHandlers(
      try(exec(rasterize(10, "max", ofile = tif), on = f), silent = TRUE),
      warning = function(w) { if (grepl("x.tif", conditionMessage(w), fixed = TRUE)) n <<- n + 1; invokeRestart("muffleWarning") })
    n
  }

  # terra and sf report the GDAL errors to R once loaded. The tests above already used invalid CRS
  expect_gt(gdal_errors(), 0)

  expect_error(exec(set_crs(12) + summarise(), on = f), "crs not found")
  expect_error(exec(set_crs("BLA") + summarise(), on = f), "WKT string")
  expect_gt(gdal_errors(), 0)
})

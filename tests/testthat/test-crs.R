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
  expect_match(las_crs$input, "Montserrat 1958 / British West Indies Grid")
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
  expect_match(las_crs$input, "Montserrat 1958 / British West Indies Grid")
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


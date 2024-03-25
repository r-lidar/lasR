test_that("Object are assigned a CRS",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  pipeline <- rasterize(5, "count") + local_maximum(5)
  ans = exec(pipeline, on = f)

  expect_match(terra::crs(ans[[1]]), "NAD83\\(CSRS\\) / MTM zone 7")
  expect_match(sf::st_crs(ans[[2]])$input, "NAD83\\(CSRS\\) / MTM zone 7")
})

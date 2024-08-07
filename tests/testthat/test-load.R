test_that("load raster works",
{
  r = system.file("extdata/bcts", "bcts_dsm_5m.tif", package = "lasR")
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  rr =  load_raster(r)
  pipeline <- rr + pit_fill(rr)
  ans1 = exec(pipeline, on = f, verbose = F)

  rr = chm(5)
  pipeline <- rr + pit_fill(rr)
  ans = exec(pipeline, on = f, verbose = F)
  ans2 = ans[[2]]

  expect_equal(ans1[], ans2[], tolerance = 3e-5, ignore_attr = TRUE)

  # terra::plot(terra::rast(r), col = lidR::height.colors(25))
  # terra::plot(ans[[1]], col = lidR::height.colors(25))
  # terra::plot(terra::rast(r)- ans[[1]])
  # terra::plot(ans1)
  # terra::plot(ans2)
})


test_that("load raster fails",
{
  r = system.file("extdata/bcts", "bcts_dsm_5m.tif", package = "lasR")
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  pipeline =  load_raster(f[1])
  expect_error(suppressWarnings(exec(pipeline, on = f)))

  pipeline =  load_raster(r, 2)
  expect_error(exec(pipeline, on = f), "beyond the number of bands")

  u <- system.file("extdata", "Topography.las", package = "lasR")
  r = exec(rasterize(2), on = u, noread = TRUE)
  r
  expect_error(exec(load_raster(r,band=1L), on=f), "non overlaping bounding boxes")
})

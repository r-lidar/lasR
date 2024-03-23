test_that("load raster works",
{
  r = system.file("extdata/bcts", "bcts_dsm_5m.tif", package = "lasR")
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  rr =  load_raster(r)
  pipeline <- rr + pit_fill(rr, ofile = "/tmp/pitfill.tif")
  ans1 = exec(pipeline, on = f, verbose = F)

  rr = chm(5)
  pipeline <- rr + pit_fill(rr, ofile = "/tmp/pitfill2.tif")
  ans = exec(pipeline, on = f, verbose = F)
  ans2 = ans[[2]]

  expect_equal(ans1[], ans2[], tolerance = 3e-5, ignore_attr = TRUE)

  # terra::plot(terra::rast(r), col = lidR::height.colors(25))
  # terra::plot(ans[[1]], col = lidR::height.colors(25))
  # terra::plot(terra::rast(r)- ans[[1]])
  # terra::plot(ans1)
  # terra::plot(ans2)
})

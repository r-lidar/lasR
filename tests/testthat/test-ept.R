test_that("EPT local read matches LAS source",
{
  ept <- system.file("extdata", "ept-test", "ept.json", package = "lasR")
  las <- system.file("extdata", "Example.las", package = "lasR")

  oept <- paste0(tempdir(), "/ept_test.las")
  olas <- paste0(tempdir(), "/las_test.las")

  exec(reader() + write_las(oept), on = ept)
  exec(reader() + write_las(olas), on = las)

  ans_ept <- exec(reader() + summarise(), on = oept)
  ans_las <- exec(reader() + summarise(), on = olas)

  expect_equal(ans_ept$npoints, ans_las$npoints)
  expect_equal(ans_ept$epsg, ans_las$epsg)
})

test_that("EPT reader detects non-laszip dataType",
{
  dir <- tempdir()
  ept_dir <- file.path(dir, "ept-bad")
  dir.create(ept_dir, showWarnings = FALSE)

  ept_json <- list(
    bounds = c(0, 0, 0, 10, 10, 10),
    boundsConforming = c(0, 0, 0, 10, 10, 10),
    dataType = "binary",
    hierarchyType = "json",
    schema = list(list(name = "X", type = "signed", size = 4)),
    span = 128
  )

  jsonlite::write_json(ept_json, file.path(ept_dir, "ept.json"), auto_unbox = TRUE)

  expect_error(
    exec(reader() + summarise(), on = file.path(ept_dir, "ept.json")),
    "only 'laszip' is supported"
  )
})

test_that("EPT pipeline integration works",
{
  ept <- system.file("extdata", "ept-test", "ept.json", package = "lasR")

  ofile <- paste0(tempdir(), "/ept_raster.tif")
  pipeline <- reader() + rasterize(5, "zmax", ofile = ofile)
  ans <- exec(pipeline, on = ept)

  expect_true(file.exists(ofile))
})

test_that("EPT spatial query fetches only needed tiles",
{
  ept <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  las <- system.file("extdata", "Topography.las", package = "lasR")

  # Full read should match source
  ofile <- paste0(tempdir(), "/ept_multi_full.las")
  exec(reader() + write_las(ofile), on = ept)
  full <- exec(reader() + summarise(), on = ofile)
  las_full <- exec(reader() + summarise(), on = las)
  expect_equal(full$npoints, las_full$npoints)

  # Query bottom-left quadrant only — should get exactly 18806 points (1 tile)
  ofile2 <- paste0(tempdir(), "/ept_multi_bl.las")
  exec(reader_rectangles(273360, 5274360, 273490, 5274490) + write_las(ofile2), on = ept)
  quad <- exec(reader() + summarise(), on = ofile2)
  expect_equal(quad$npoints, 18806)

  # Query top-right quadrant — should get exactly 23306 points (1 tile)
  ofile3 <- paste0(tempdir(), "/ept_multi_tr.las")
  exec(reader_rectangles(273510, 5274510, 273640, 5274640) + write_las(ofile3), on = ept)
  quad2 <- exec(reader() + summarise(), on = ofile3)
  expect_equal(quad2$npoints, 23306)
})

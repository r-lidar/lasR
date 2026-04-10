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

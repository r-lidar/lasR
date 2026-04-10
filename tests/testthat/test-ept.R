test_that("EPT local read matches LAS source",
{
  ept <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  las <- system.file("extdata", "Topography.las", package = "lasR")

  ofile <- paste0(tempdir(), "/ept_full.las")
  exec(reader() + write_las(ofile), on = ept)
  full <- exec(reader() + summarise(), on = ofile)
  las_full <- exec(reader() + summarise(), on = las)
  expect_equal(full$npoints, las_full$npoints)
})

test_that("EPT spatial query fetches only needed tiles",
{
  ept <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")

  ofile <- paste0(tempdir(), "/ept_bl.las")
  exec(reader_rectangles(273360, 5274360, 273490, 5274490) + write_las(ofile), on = ept)
  quad <- exec(reader() + summarise(), on = ofile)
  expect_equal(quad$npoints, 18806)

  ofile2 <- paste0(tempdir(), "/ept_tr.las")
  exec(reader_rectangles(273510, 5274510, 273640, 5274640) + write_las(ofile2), on = ept)
  quad2 <- exec(reader() + summarise(), on = ofile2)
  expect_equal(quad2$npoints, 23306)
})

test_that("EPT reader detects non-laszip dataType",
{
  ept_dir <- file.path(tempdir(), "ept-bad")
  dir.create(ept_dir, showWarnings = FALSE)

  writeLines('{"bounds":[0,0,0,10,10,10],"boundsConforming":[0,0,0,10,10,10],"dataType":"binary","hierarchyType":"json","schema":[{"name":"X","type":"signed","size":4}],"span":128}',
    file.path(ept_dir, "ept.json"))

  expect_error(
    exec(reader(), on = file.path(ept_dir, "ept.json")),
    "laszip"
  )
})

test_that("EPT pipeline integration works",
{
  ept <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")

  ofile <- paste0(tempdir(), "/ept_raster.tif")
  pipeline <- reader() + rasterize(5, "zmax", ofile = ofile)
  ans <- exec(pipeline, on = ept)

  expect_true(file.exists(ofile))
})

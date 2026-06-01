# Ground-truth reprojections used in the assertions below were verified independently
# with the command-line tool `gdaltransform` (PROJ/GDAL). Topography.las is in
# NAD83(CSRS) / MTM zone 7 (EPSG:2949), located in Quebec, Canada.
#
# These tests deliberately rely on lasR-native readers (summarise/callback with
# noread = TRUE) so they do not depend on terra/sf being loadable. The optional
# rasterization test uses terra and is skipped if terra is unavailable.

read_crs   = function(f) { exec(summarise(), on = f, noread = TRUE)$crs }
read_epsg  = function(f) { exec(summarise(), on = f, noread = TRUE)$epsg }
read_range = function(f)
{
  exec(callback(function(d) c(xmin = min(d$X), xmax = max(d$X),
                              ymin = min(d$Y), ymax = max(d$Y),
                              zmin = min(d$Z), zmax = max(d$Z)), expose = "xyz"),
       on = f, noread = TRUE)
}

test_that("transform_crs reprojects to a geographic CRS (EPSG:4326)",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")
  out <- tempfile(fileext = ".las")

  exec(reader_las() + transform_crs(4326) + write_las(out), on = f, noread = TRUE)

  expect_equal(read_epsg(out), 4326L)
  expect_match(read_crs(out), "WGS 84")

  r <- read_range(out)
  # Expected lon/lat (gdaltransform): lon ~ -70.918..-70.914, lat ~ 47.6076..47.6102
  expect_gt(unname(r["xmin"]), -70.93)
  expect_lt(unname(r["xmax"]), -70.90)
  expect_gt(unname(r["ymin"]), 47.60)
  expect_lt(unname(r["ymax"]), 47.62)
})

test_that("transform_crs reprojects to a projected CRS (EPSG:32619) and preserves Z",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")
  src <- read_range(f)
  out <- tempfile(fileext = ".las")

  exec(reader_las() + transform_crs(32619) + write_las(out), on = f, noread = TRUE)

  expect_equal(read_epsg(out), 32619L)
  expect_match(read_crs(out), "UTM zone 19N")

  r <- read_range(out)
  # Expected UTM 19N (gdaltransform): center ~ 355975, 5274614
  expect_gt(unname(r["xmin"]), 355800)
  expect_lt(unname(r["xmax"]), 356150)
  expect_gt(unname(r["ymin"]), 5274400)
  expect_lt(unname(r["ymax"]), 5274800)

  # Horizontal-only reprojection: Z is preserved exactly
  expect_equal(unname(r["zmin"]), unname(src["zmin"]))
  expect_equal(unname(r["zmax"]), unname(src["zmax"]))
})

test_that("transform_crs moves coordinates whereas set_crs only relabels",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")
  src <- read_range(f)

  # set_crs: coordinates are unchanged
  o1 <- tempfile(fileext = ".las")
  exec(reader_las() + set_crs(32619) + write_las(o1), on = f, noread = TRUE)
  r1 <- read_range(o1)
  expect_equal(unname(r1["xmin"]), unname(src["xmin"]))
  expect_equal(unname(r1["ymin"]), unname(src["ymin"]))

  # transform_crs: coordinates are reprojected (changed)
  o2 <- tempfile(fileext = ".las")
  exec(reader_las() + transform_crs(32619) + write_las(o2), on = f, noread = TRUE)
  r2 <- read_range(o2)
  expect_false(isTRUE(all.equal(unname(r2["xmin"]), unname(src["xmin"]))))
  expect_false(isTRUE(all.equal(unname(r2["ymin"]), unname(src["ymin"]))))
})

test_that("transform_crs is a near-identity when target equals source",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")
  src <- read_range(f)
  out <- tempfile(fileext = ".las")

  exec(reader_las() + transform_crs(2949) + write_las(out), on = f, noread = TRUE)

  expect_equal(read_epsg(out), 2949L)
  r <- read_range(out)
  expect_equal(unname(r["xmin"]), unname(src["xmin"]), tolerance = 0.01)
  expect_equal(unname(r["ymax"]), unname(src["ymax"]), tolerance = 0.01)
})

test_that("transform_crs accepts a WKT string",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")
  wkt <- 'GEOGCS["WGS 84",DATUM["WGS_1984",SPHEROID["WGS 84",6378137,298.257223563,AUTHORITY["EPSG","7030"]],AUTHORITY["EPSG","6326"]],PRIMEM["Greenwich",0,AUTHORITY["EPSG","8901"]],UNIT["degree",0.0174532925199433,AUTHORITY["EPSG","9122"]],AXIS["Latitude",NORTH],AXIS["Longitude",EAST],AUTHORITY["EPSG","4326"]]'
  out <- tempfile(fileext = ".las")

  exec(reader_las() + transform_crs(wkt) + write_las(out), on = f, noread = TRUE)

  expect_equal(read_epsg(out), 4326L)
  r <- read_range(out)
  expect_gt(unname(r["xmin"]), -70.93)
  expect_lt(unname(r["xmax"]), -70.90)
})

test_that("transform_crs fails with an invalid target CRS",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  expect_error(exec(reader_las() + transform_crs(12) + write_las(), on = f, noread = TRUE))
})

test_that("transform_crs reprojects the coverage extent for downstream rasterization",
{
  # Exercises the parser-level extent propagation: if the coverage extent were not
  # reprojected, the (master) raster would be allocated in the source CRS and the
  # reprojected points would fall outside it, producing an empty raster.
  skip_if_not_installed("terra")

  f <- system.file("extdata", "Topography.las", package = "lasR")
  tif <- tempfile(fileext = ".tif")

  exec(reader_las() + transform_crs(32619) + rasterize(10, "count", ofile = tif), on = f)

  r <- terra::rast(tif)
  e <- as.vector(terra::ext(r))
  expect_gt(e[["xmin"]], 355000)
  expect_lt(e[["xmax"]], 357000)
  expect_gt(e[["ymin"]], 5274000)
  expect_lt(e[["ymax"]], 5275000)
  # The raster is correctly placed, so it actually contains the points
  expect_gt(sum(terra::values(r), na.rm = TRUE), 0)
})

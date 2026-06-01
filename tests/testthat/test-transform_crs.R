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

read_range_xyz = function(f, pipeline = NULL)
{
  cb = callback(function(d) c(xmin = min(d$X), xmax = max(d$X),
                              ymin = min(d$Y), ymax = max(d$Y),
                              zmin = min(d$Z), zmax = max(d$Z),
                              n = length(d$X),
                              nfinite = sum(is.finite(d$X) & is.finite(d$Y) & is.finite(d$Z))),
                expose = "xyz")
  if (is.null(pipeline)) exec(cb, on = f, noread = TRUE)
  else exec(pipeline + cb, on = f, noread = TRUE)
}

test_that("transform_crs reprojects PCD float coordinates without corrupting them",
{
  # PCD stores X/Y/Z as raw float/double (not scaled int32) and carries no CRS. Writing
  # reprojected coordinates with the int32 setter used to reinterpret the float bytes and
  # produce NaN. Assign a geographic CRS then reproject to Web Mercator.
  f <- system.file("extdata", "pcd_ascii.pcd", package = "lasR")

  src <- read_range_xyz(f)
  out <- read_range_xyz(f, set_crs(4326) + transform_crs(3857))

  # No corruption (the bug produced NaN) and no points spuriously dropped.
  expect_equal(unname(out["nfinite"]), unname(out["n"]))
  expect_equal(unname(out["n"]), unname(src["n"]))
  expect_true(all(is.finite(out)))

  # Coordinates were really reprojected from degrees to metres (magnitudes blow up).
  expect_gt(abs(unname(out["xmin"])), 1e6)
  expect_gt(abs(unname(out["ymin"])), 1e5)

  # Z (elevation) is preserved unchanged.
  expect_equal(unname(out["zmin"]), unname(src["zmin"]), tolerance = 1e-4)
  expect_equal(unname(out["zmax"]), unname(src["zmax"]), tolerance = 1e-4)
})

test_that("transform_crs writes reprojected PCD float coordinates correctly to LAS",
{
  # Regression for the PCD-float -> LAS path. The in-memory float schema keeps identity
  # scale/offset (so callbacks/get_x read the coordinate directly), but write_las() quantizes
  # to LAS int32. If it used the float schema's 1.0 scale, sub-degree lon/lat would all collapse
  # to 0. transform_crs records a target-appropriate scale/offset on the header and the LAS
  # writer uses it for float/double axes. Here the PCD coords are treated as Web Mercator metres
  # then reprojected to lon/lat, giving tiny sub-degree values that expose the bug.
  f <- system.file("extdata", "pcd_ascii.pcd", package = "lasR")
  inmem <- read_range_xyz(f, set_crs(3857) + transform_crs(4326))

  o <- tempfile(fileext = ".las")
  on.exit(unlink(o), add = TRUE)
  exec(reader_las() + set_crs(3857) + transform_crs(4326) + write_las(o), on = f, noread = TRUE)
  onlas <- read_range_xyz(o)

  expect_equal(unname(onlas["n"]), unname(inmem["n"]))
  # Reprojected lon/lat are sub-degree but non-zero; the bug collapsed every coordinate to 0.
  expect_gt(abs(unname(onlas["xmin"])), 1e-5)
  expect_gt(abs(unname(onlas["ymin"])), 1e-6)
  expect_false(isTRUE(all.equal(unname(onlas["xmin"]), unname(onlas["xmax"]))))
  # And they match the in-memory reprojected coordinates within LAS quantization (~1e-7). The
  # values are tiny, so compare with an absolute (not relative) tolerance.
  expect_lt(abs(unname(onlas["xmin"]) - unname(inmem["xmin"])), 1e-6)
  expect_lt(abs(unname(onlas["xmax"]) - unname(inmem["xmax"])), 1e-6)
  expect_lt(abs(unname(onlas["ymin"]) - unname(inmem["ymin"])), 1e-6)
  expect_lt(abs(unname(onlas["ymax"]) - unname(inmem["ymax"])), 1e-6)
})

test_that("transform_crs keeps projected precision for projected PCD writes to LAS",
{
  # Projected -> projected from a float source. The PCD schema scale is a placeholder (1.0), so
  # transform_crs must pick a fine projected scale (1 cm) for the LAS quantization rather than
  # reuse it. Otherwise the LAS output is quantized to whole units (~0.3 m error here). (For an
  # INT32/LAS source the real schema scale is reused, exercised by the other tests above.)
  f <- system.file("extdata", "pcd_ascii.pcd", package = "lasR")
  inmem <- read_range_xyz(f, set_crs(3857) + transform_crs(32619))

  o <- tempfile(fileext = ".las")
  on.exit(unlink(o), add = TRUE)
  exec(reader_las() + set_crs(3857) + transform_crs(32619) + write_las(o), on = f, noread = TRUE)
  onlas <- read_range_xyz(o)

  expect_equal(unname(onlas["n"]), unname(inmem["n"]))
  # Round-trip preserves the reprojected coordinates to centimetre level, not whole units.
  expect_lt(abs(unname(onlas["xmin"]) - unname(inmem["xmin"])), 0.02)
  expect_lt(abs(unname(onlas["xmax"]) - unname(inmem["xmax"])), 0.02)
  expect_lt(abs(unname(onlas["ymin"]) - unname(inmem["ymin"])), 0.02)
  expect_lt(abs(unname(onlas["ymax"]) - unname(inmem["ymax"])), 0.02)
})

test_that("transform_crs scales the tile buffer to the target CRS units",
{
  # triangulate() requires a 20 (source-metre) buffer. After reprojecting metres -> degrees
  # that buffer must be expressed in degrees (~1.8e-4) for the downstream rasterize halo; if
  # it stayed at 20 it would be read as 20 degrees and the master raster would balloon by
  # ceil(20 / 1e-4) ~ 1e5 pixels per side (out of memory). The fix keeps the raster small.
  skip_if_not_installed("terra")

  f <- system.file("extdata", "Topography.las", package = "lasR")
  tif <- tempfile(fileext = ".tif")

  exec(reader_las() + transform_crs(4326) + triangulate() + rasterize(0.0002, "max", ofile = tif), on = f)

  r <- terra::rast(tif)
  # ~0.004 deg coverage at 0.0002 deg => ~20 px plus a small (correctly scaled) halo.
  expect_lt(terra::ncol(r), 1000)
  expect_lt(terra::nrow(r), 1000)
  expect_gt(terra::ncell(r), 0)
})

test_that("transform_crs then COPC write sizes the octree in the target CRS",
{
  # A merged COPC write sizes its octree from the catalog (input) bbox, which is in the
  # source CRS. After transform_crs the points are in the target CRS, so that bbox must be
  # reprojected; otherwise every reprojected point falls outside the octree extent and the
  # output is structurally broken (clamped / empty).
  f <- system.file("extdata", "Topography.las", package = "lasR")
  o <- tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  src_n <- exec(reader_las() + summarise(), on = f)$npoints
  expect_error(exec(reader_las() + transform_crs(4326) + write_las(o, experimental_writer = TRUE), on = f), NA)

  # Re-read the COPC: all points survive (a mis-sized octree would clamp/drop them) and
  # the coordinates are the reprojected lon/lat, not the source metres.
  expect_equal(read_epsg(o), 4326L)
  expect_equal(exec(reader_las() + summarise(), on = o)$npoints, src_n)

  r <- read_range_xyz(o)
  expect_gt(unname(r["xmin"]), -70.93)
  expect_lt(unname(r["xmax"]), -70.90)
  expect_gt(unname(r["ymin"]), 47.60)
  expect_lt(unname(r["ymax"]), 47.62)
})

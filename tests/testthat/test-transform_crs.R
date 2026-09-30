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

# First points of pcd_ascii.pcd as stored in memory (float32), reprojected with gdaltransform
# (GDAL 3.8.4), e.g.
# echo "-121.08580017089844 -24.701700210571289" | gdaltransform -s_srs EPSG:3857 -t_srs EPSG:32619
pcd_first_3857_to_32619 = rbind(
  c(11307805.660534, -70.084096),
  c(11307806.093565, -70.068780),
  c(11307806.075293, -68.985245),
  c(11307805.326651, -68.482200),
  c(11307805.508877, -68.541214))

# echo "-121.08580017089844 -24.701700210571289" | gdaltransform -s_srs EPSG:4326 -t_srs EPSG:3857
pcd_first_4326_to_3857 = rbind(
  c(-13479209.617321, -2839149.455157),
  c(-13462333.170775, -2838487.834834),
  c(-13463045.733739, -2791766.151913),
  c(-13492222.596736, -2770123.289379),
  c(-13485120.747508, -2772660.732461))

read_first_xy = function(f, pipeline = NULL, n = 5)
{
  cb = callback(function(d) unname(as.matrix(d[seq_len(n), c("X", "Y")])), expose = "xy")
  if (is.null(pipeline)) exec(cb, on = f)
  else exec(pipeline + cb, on = f)
}

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

  # And they are not rounded to the precision of a float32 (1 m at these magnitudes).
  xy <- read_first_xy(f, set_crs(4326) + transform_crs(3857))
  expect_lt(max(abs(xy - pcd_first_4326_to_3857)), 1e-3)

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
  # Projected -> projected from a float32 source (PCD TYPE F SIZE 4). The reprojected eastings
  # (~1.13e7) are beyond the precision of a float32 (1 m), so X/Y must be promoted to double in
  # memory. The PCD schema scale is a placeholder (1.0), so transform_crs must also pick a fine
  # projected scale (1 cm) for the LAS quantization rather than reuse it. Both are checked against
  # gdaltransform on both axes with an absolute tolerance.
  f <- system.file("extdata", "pcd_ascii.pcd", package = "lasR")
  pipeline <- set_crs(3857) + transform_crs(32619)

  inmem <- read_first_xy(f, pipeline)
  expect_lt(max(abs(inmem - pcd_first_3857_to_32619)), 1e-3)

  o <- tempfile(fileext = ".las")
  on.exit(unlink(o), add = TRUE)
  exec(reader_las() + pipeline + write_las(o), on = f, noread = TRUE)

  expect_equal(unname(read_range_xyz(o)["n"]), unname(read_range_xyz(f)["n"]))
  onlas <- read_first_xy(o)
  expect_lt(max(abs(onlas - pcd_first_3857_to_32619)), 0.01)
})

test_that("transform_crs keeps the other attributes when promoting float X/Y to double",
{
  # Promoting X/Y from float to double re-lays out every point: the attributes stored after X/Y
  # (Z, intensity, gpstime, ...) are moved and must be preserved.
  f <- system.file("extdata", "Example.pcd", package = "lasR")
  read_all <- function(pipeline = NULL)
  {
    cb <- callback(function(d) d, expose = "*", no_las_update = TRUE)
    if (is.null(pipeline)) exec(cb, on = f) else exec(pipeline + cb, on = f)
  }

  src <- read_all()
  out <- read_all(set_crs(32617) + transform_crs(32618))

  expect_equal(nrow(out), nrow(src))
  for (name in setdiff(names(src), c("X", "Y")))
    expect_equal(out[[name]], src[[name]], label = name)

  # First point (339002.88 5248000.50 stored as float32):
  # echo "339002.875 5248000.5" | gdaltransform -s_srs EPSG:32617 -t_srs EPSG:32618
  expect_lt(abs(out$X[1] - -113858.765517), 1e-3)
  expect_lt(abs(out$Y[1] - 5277949.797934), 1e-3)
})

test_that("transform_crs writes reprojected PCD float coordinates to PCD without precision loss",
{
  f <- system.file("extdata", "pcd_ascii.pcd", package = "lasR")
  n <- unname(read_range_xyz(f)["n"])

  for (binary in c(TRUE, FALSE))
  {
    o <- tempfile(fileext = ".pcd")
    exec(set_crs(3857) + transform_crs(32619) + write_pcd(o, binary = binary), on = f)

    expect_equal(unname(read_range_xyz(o)["n"]), n)
    xy <- read_first_xy(o)
    expect_lt(max(abs(xy - pcd_first_3857_to_32619)), 1e-3)
    unlink(c(o, sub("\\.pcd$", ".bbox", o)))
  }
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

test_that("transform_crs does not inflate projected buffers from geographic sources",
{
  # The inverse direction must not multiply a projected downstream buffer by the
  # degrees-to-metres ratio. That used to turn triangulate()'s 20 m halo into a
  # multi-million-metre raster buffer and fail with std::bad_alloc.
  skip_if_not_installed("terra")

  f <- system.file("extdata", "Topography.las", package = "lasR")
  geo <- tempfile(fileext = ".las")
  tif <- tempfile(fileext = ".tif")
  on.exit(unlink(c(geo, tif)), add = TRUE)

  exec(reader_las() + transform_crs(4326) + write_las(geo), on = f, noread = TRUE)
  exec(reader_las() + transform_crs(32619) + triangulate() + rasterize(10, "max", ofile = tif), on = geo)

  r <- terra::rast(tif)
  expect_lt(terra::ncol(r), 1000)
  expect_lt(terra::nrow(r), 1000)
  expect_gt(terra::ncell(r), 0)
})

test_that("transform_crs does not duplicate buffer points across buffered tiles",
{
  # A reprojection rotates the tiles, so the axis-aligned bounding box of a reprojected tile
  # also covers slivers of its neighbours. With a buffer, the points of these slivers are read
  # as buffer points. write_las() and summarise() must rely on the buffer flag set by the
  # reader in the source CRS, otherwise they are written and counted twice.
  f <- system.file("extdata", "Topography.las", package = "lasR")
  xyz <- function(x) exec(callback(function(d) d, expose = "xyz", no_las_update = TRUE), on = x)
  src <- xyz(f)

  td <- tempfile("transform_crs_tiles_")
  dir.create(td)
  on.exit(unlink(td, recursive = TRUE), add = TRUE)

  exec(reader_las() + write_las(file.path(td, "tile_*.las")), on = f, chunk = 100)
  tiles <- list.files(td, pattern = "^tile_.*\\.las$", full.names = TRUE)
  exec(write_lax(), on = tiles)
  expect_gt(length(tiles), 1)

  for (epsg in c(4326, 32619))
  {
    tpl <- file.path(td, paste0("out", epsg, "_*.las"))
    exec(reader_las() + transform_crs(epsg) + write_las(tpl), on = tiles, buffer = 30)
    outs <- list.files(td, pattern = paste0("^out", epsg, "_.*\\.las$"), full.names = TRUE)
    expect_length(outs, length(tiles))

    out <- do.call(rbind, lapply(outs, xyz))
    expect_equal(nrow(out), nrow(src))
    expect_true(identical(sort(out$Z), sort(src$Z)))

    s <- exec(reader_las() + transform_crs(epsg) + summarise(), on = tiles, buffer = 30)
    expect_equal(s$npoints, nrow(src))
  }
})

test_that("transform_crs keeps circular queries exact after a projected -> geographic transform",
{
  # After reprojecting to lon/lat the query circle is no longer a circle. Buffer points must
  # still be excluded according to the source circle.
  f <- system.file("extdata", "Topography.las", package = "lasR")
  src <- exec(callback(function(d) d, expose = "xyz", no_las_update = TRUE), on = f)

  xc <- 273500
  yc <- 5274500
  r <- 60
  expected <- sum((src$X - xc)^2 + (src$Y - yc)^2 <= r^2)

  o <- tempfile(fileext = ".las")
  on.exit(unlink(o), add = TRUE)
  exec(reader_circles(xc, yc, r) + transform_crs(4326) + write_las(o), on = f, buffer = 20)
  expect_equal(exec(summarise(), on = o, noread = TRUE)$npoints, expected)

  s <- exec(reader_circles(xc, yc, r) + transform_crs(4326) + summarise(), on = f, buffer = 20)
  expect_equal(s$npoints, expected)
})

test_that("transform_crs converts the buffers of downstream stages given in data units",
{
  # rasterize() with a window asks for a buffer in the units of the coordinates it receives:
  # degrees after transform_crs(4326), metres after transform_crs(32619). The reader works in
  # the source CRS, so this buffer must be converted into source units, and must be large
  # enough in every direction: 1 degree of latitude is longer than 1 degree of longitude.
  # The buffer points loaded by the reader are exposed by the callback ('b' = buffer flag).
  f <- system.file("extdata", "Topography.las", package = "lasR")

  halo <- function(d)
  {
    core <- d$Buffer == 0
    c(cxmin = min(d$X[core]), cxmax = max(d$X[core]), cymin = min(d$Y[core]), cymax = max(d$Y[core]),
      xmin = min(d$X), xmax = max(d$X), ymin = min(d$Y), ymax = max(d$Y))
  }

  # For each side of each chunk that has at least 'need' of data beyond it, the reader must have
  # loaded buffer points up to 'need' away (minus the point spacing).
  expect_halo <- function(res, need)
  {
    res <- do.call(rbind, res)
    expect_gt(nrow(res), 1)
    ext <- c(min(res[, "xmin"]), max(res[, "xmax"]), min(res[, "ymin"]), max(res[, "ymax"]))
    w <- res[, "cxmin"] - ext[1] >= need
    e <- ext[2] - res[, "cxmax"] >= need
    s <- res[, "cymin"] - ext[3] >= need
    n <- ext[4] - res[, "cymax"] >= need
    expect_true(any(w) && any(e) && any(s) && any(n))
    halos <- c((res[, "cxmin"] - res[, "xmin"])[w], (res[, "xmax"] - res[, "cxmax"])[e],
               (res[, "cymin"] - res[, "ymin"])[s], (res[, "ymax"] - res[, "cymax"])[n])
    expect_gt(min(halos), 0.95 * need)
  }

  # Projected -> geographic. need_buffer = (1.1e-3 - 1e-4) / 2 = 5e-4 degrees.
  res <- exec(reader_las() + transform_crs(4326) + rasterize(c(1e-4, 1.1e-3), "max", ofile = "") +
              callback(halo, expose = "xyzb", drop_buffer = FALSE), on = f, chunk = 100)
  expect_halo(res, 5e-4)

  # Geographic -> projected. need_buffer = (101 - 1) / 2 = 50 metres.
  geo <- tempfile(fileext = ".las")
  on.exit(unlink(c(geo, sub("\\.las$", ".lax", geo))), add = TRUE)
  exec(reader_las() + transform_crs(4326) + write_las(geo), on = f)
  exec(write_lax(), on = geo)
  res <- exec(reader_las() + transform_crs(32619) + rasterize(c(1, 101), "max", ofile = "") +
              callback(halo, expose = "xyzb", drop_buffer = FALSE), on = geo, chunk = 0.0013)
  expect_halo(res, 50)
})

test_that("transform_crs keeps rasters of circular queries inside the circle",
{
  # The chunk of a circular query stays circular after transform_crs, and rasters are masked
  # with the reprojected circle: no cell outside the query circle may hold a value, and the
  # circle is covered as without transform_crs.
  skip_if_not_installed("terra")
  f <- system.file("extdata", "Topography.las", package = "lasR")
  xc <- 273500
  yc <- 5274500
  r <- 60
  tif <- tempfile(fileext = ".tif")
  on.exit(unlink(tif), add = TRUE)

  # Area covered by the cells with a value, in source square metres
  area <- function(rr, epsg)
  {
    e <- as.vector(terra::ext(rr))
    p <- rbind(c(e[1], e[3]), c(e[2], e[3]), c(e[1], e[4]))
    if (!is.na(epsg)) p <- terra::project(p, paste0("EPSG:", epsg), "EPSG:2949")
    w <- sqrt(sum((p[2, ] - p[1, ])^2))
    h <- sqrt(sum((p[3, ] - p[1, ])^2))
    sum(!is.na(terra::values(rr)[, 1])) * w * h / terra::ncell(rr)
  }

  exec(reader_circles(xc, yc, r) + rasterize(2, "max", ofile = tif), on = f, buffer = 20)
  expected <- area(terra::rast(tif), NA)
  unlink(tif)

  for (epsg in c(32619, 4326))
  {
    res <- if (epsg == 4326) 2.5e-5 else 2
    exec(reader_circles(xc, yc, r) + transform_crs(epsg) + rasterize(res, "max", ofile = tif), on = f, buffer = 20)
    rr <- terra::rast(tif)
    valid <- !is.na(terra::values(rr)[, 1])
    xy <- terra::xyFromCell(rr, which(valid))
    back <- terra::project(xy, paste0("EPSG:", epsg), "EPSG:2949")
    d <- sqrt((back[, 1] - xc)^2 + (back[, 2] - yc)^2)
    expect_lte(max(d), r + 4) # 2 cells of tolerance, the same as without transform_crs
    expect_equal(area(rr, epsg), expected, tolerance = 0.05)
    unlink(tif)
  }
})

test_that("transform_crs does not corrupt rasters at the seams of rotated tiles",
{
  # The reprojected tiles are rotated, so their bounding boxes overlap. The raster of a tile
  # must not overwrite the cells of its neighbours with NA or with values computed from an
  # incomplete neighbourhood. A tiled run must give the same raster as a single chunk.
  skip_if_not_installed("terra")
  f <- system.file("extdata", "Topography.las", package = "lasR")

  td <- tempfile("transform_crs_rtiles_")
  dir.create(td)
  on.exit(unlink(td, recursive = TRUE), add = TRUE)

  exec(reader_las() + write_las(file.path(td, "tile_*.las")), on = f, chunk = 100)
  tiles <- list.files(td, pattern = "^tile_.*\\.las$", full.names = TRUE)
  exec(write_lax(), on = tiles)
  expect_gt(length(tiles), 1)

  o1 <- file.path(td, "single.tif")
  o2 <- file.path(td, "tiled.tif")
  for (epsg in c(32619, 3857))
  {
    pipe <- function(o) reader_las() + transform_crs(epsg) + rasterize(2, "max", ofile = o)
    exec(pipe(o1), on = f)
    exec(pipe(o2), on = tiles)
    a <- terra::values(terra::rast(o1))[, 1]
    b <- terra::values(terra::rast(o2))[, 1]
    expect_equal(length(a), length(b))
    expect_equal(sum(is.na(b) & !is.na(a)), 0) # no holes
    expect_equal(sum(abs(b - a) > 1e-6, na.rm = TRUE), 0) # no wrong values
  }
})

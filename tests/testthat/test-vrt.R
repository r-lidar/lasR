files = paste0(system.file(package = "lasR"), "/extdata/bcts/")
files = list.files(files, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

# Paths can be written differently for the same file (e.g. /var vs /private/var on macOS)
same_path = function(a, b) normalizePath(a) == normalizePath(b)

test_that("a .vrt output writes one file per chunk combined into a VRT",
{
  d = tempfile()
  dir.create(d)
  vfile = file.path(d, "dtm.vrt")

  vrt = rasterize(1, c("min", "max"), ofile = vfile)
  ref = rasterize(1, c("min", "max"))
  ans = exec(reader() + vrt + ref, on = files)

  # One file per chunk named after the VRT
  tiles = paste0("bcts_", 1:4, "_dtm.tif")
  expect_setequal(list.files(d), c("dtm.vrt", tiles))

  # A single raster is returned and it is the VRT
  v = ans[[1]]
  expect_s4_class(v, "SpatRaster")
  expect_true(same_path(terra::sources(v), vfile))
  expect_equal(names(v), c("min", "max"))

  # Same raster than when written in a single file
  expect_true(terra::compareGeom(v, ans[[2]]))
  expect_equal(terra::values(v), terra::values(ans[[2]]))

  # Paths are relative to the VRT so the folder can be moved
  xml = readLines(vfile)
  src = grep("SourceFilename", xml, value = TRUE)
  expect_true(all(grepl("relativeToVRT=\"1\"", src)))
})

test_that("a .vrt output works with files processed in parallel",
{
  skip_if_not(has_omp_support())

  vfile = tempfile(fileext = ".vrt")
  vrt = rasterize(1, "min", ofile = vfile)
  ref = rasterize(1, "min")
  ans = exec(reader() + vrt + ref, on = files, ncores = concurrent_files(2))

  expect_true(same_path(terra::sources(ans[[1]]), vfile))
  expect_equal(terra::values(ans[[1]]), terra::values(ans[[2]]))
})

test_that("two VRTs in the same folder do not overwrite each other",
{
  d = tempfile()
  dir.create(d)

  dtm = rasterize(1, "min", ofile = file.path(d, "dtm.VRT"))
  dsm = rasterize(1, "max", ofile = file.path(d, "dsm.vrt"))
  ans = exec(reader() + dtm + dsm, on = files)

  expect_length(list.files(d, pattern = "_dtm\\.tif$"), 4L)
  expect_length(list.files(d, pattern = "_dsm\\.tif$"), 4L)
  expect_true(all(terra::values(ans[[2]]) >= terra::values(ans[[1]]), na.rm = TRUE))
  expect_false(isTRUE(all.equal(terra::values(ans[[1]]), terra::values(ans[[2]]))))
})

test_that("a .vrt output works for a raster stage that needs a buffer",
{
  r = rasterize(1, "max", ofile = "")
  vfile = tempfile(fileext = ".vrt")
  vrt = lasR:::focal(r, 4, fun = "mean", ofile = vfile)
  ref = lasR:::focal(r, 4, fun = "mean")
  ans = exec(reader() + r + vrt + ref, on = files)

  expect_true(same_path(terra::sources(ans[[1]]), vfile))
  expect_equal(terra::values(ans[[1]]), terra::values(ans[[2]]))
})

test_that("a .vrt output name cannot contain a wildcard",
{
  vrt = rasterize(1, "min", ofile = file.path(tempdir(), "*_dtm.vrt"))
  expect_error(exec(reader() + vrt, on = files), "wildcard")
})

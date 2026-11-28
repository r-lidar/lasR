f <- system.file("extdata", "Topography.las", package = "lasR")

is_cog = function(file)
{
  d = terra::describe(file)
  any(grepl("LAYOUT=COG", d))
}

test_that("write_cog writes a COG identical to the source raster",
{
  ofile = tempfile(fileext = ".tif")
  chm = rasterize(0.25, c("max", "min"))
  cog = write_cog(chm, ofile = ofile)
  ans = exec(reader() + chm + cog, on = f)

  expect_length(ans, 2L)
  expect_true(file.exists(ofile))
  expect_equal(terra::sources(ans[[2]]), ofile)
  expect_true(is_cog(ofile))
  expect_false(is_cog(terra::sources(ans[[1]])))

  expect_equal(names(ans[[2]]), c("max", "min"))
  expect_equal(terra::crs(ans[[2]]), terra::crs(ans[[1]]))
  expect_equal(as.vector(terra::ext(ans[[2]])), as.vector(terra::ext(ans[[1]])))
  expect_equal(terra::values(ans[[2]]), terra::values(ans[[1]]))

  # 1144 x 1144 pixels with 512 x 512 blocks: there are overviews
  d = terra::describe(ofile)
  expect_true(any(grepl("Overviews", d)))
  expect_true(any(grepl("ZSTD", d)))
})

test_that("write_cog works with a source raster written in a user defined file",
{
  src = tempfile(fileext = ".tif")
  ofile = tempfile(fileext = ".tif")
  chm = rasterize(2, "max", ofile = src)
  cog = write_cog(chm, ofile = ofile, resampling = "nearest")
  ans = exec(reader() + chm + cog, on = f)

  expect_equal(terra::sources(ans[[1]]), src)
  expect_true(is_cog(ofile))
  expect_equal(terra::values(ans[[2]]), terra::values(ans[[1]]))
})

test_that("write_cog works with a raster derived from another raster stage",
{
  ofile = tempfile(fileext = ".tif")
  chm = rasterize(2, "max")
  chm2 = lasR:::focal(chm, 8, fun = "max")
  cog = write_cog(chm2, ofile = ofile)
  ans = exec(reader() + chm + chm2 + cog, on = f)

  expect_length(ans, 3L)
  expect_true(is_cog(ofile))
  expect_equal(terra::values(ans[[3]]), terra::values(ans[[2]]))
})

test_that("write_cog works on a collection of files processed in parallel",
{
  skip_if_not(has_omp_support())

  files = paste0(system.file(package = "lasR"), "/extdata/bcts/")
  files = list.files(files, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  ofile = tempfile(fileext = ".tif")
  dtm = rasterize(1, "min")
  cog = write_cog(dtm, ofile = ofile)
  ans = exec(reader() + dtm + cog, on = files, ncores = concurrent_files(2))

  expect_true(is_cog(ofile))
  expect_equal(terra::values(ans[[2]]), terra::values(ans[[1]]))
})

test_that("write_cog fails with invalid arguments",
{
  chm = rasterize(2, "max")
  run = function(cog) exec(reader() + chm + cog, on = f)

  expect_error(write_cog(reader(), tempfile(fileext = ".tif")), "must be a raster stage")

  # The validity of the parameters is checked by the stage when the pipeline is parsed
  expect_error(run(write_cog(chm, "")), "file name is required")
  expect_error(run(write_cog(chm, paste0(tempdir(), "/*.tif"))), "wildcard")
  expect_error(run(write_cog(chm, tempfile(fileext = ".gpkg"))), ".tif")
  expect_error(run(write_cog(chm, tempfile(fileext = ".tif"), resampling = "foo")), "resampling")

  ofile = tempfile(fileext = ".TIF")
  expect_error(run(write_cog(chm, ofile, resampling = "CUBIC")), NA)
  expect_true(is_cog(ofile))
})

test_that("write_cog fails when the source raster is not a single file",
{
  # In memory raster
  chm = rasterize(2, "max", ofile = "")
  cog = write_cog(chm, ofile = tempfile(fileext = ".tif"))
  expect_error(exec(reader() + chm + cog, on = f), "not written on disk")

  # One file per chunk
  chm = rasterize(2, "max", ofile = paste0(tempdir(), "/*_cogtest.tif"))
  cog = write_cog(chm, ofile = tempfile(fileext = ".tif"))
  expect_error(exec(reader() + chm + cog, on = f), "one file per chunk")

  # Same file as the source
  src = tempfile(fileext = ".tif")
  chm = rasterize(2, "max", ofile = src)
  cog = write_cog(chm, ofile = src)
  expect_error(exec(reader() + chm + cog, on = f), "must be different")
})

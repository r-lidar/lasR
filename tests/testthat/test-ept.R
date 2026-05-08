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

test_that("EPT depth filtering works",
{
  ept <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")

  # Test data has no 0-0-0-0 tile (hierarchy starts at depth 1), so depth=0
  # yields zero points and must not hang.
  d0 <- exec(reader(depth = 0) + summarise(), on = ept)
  full <- exec(reader() + summarise(), on = ept)

  expect_equal(d0$npoints, 0)
  expect_gt(full$npoints, 0)
})

test_that("Multiple EPT endpoints produce an error",
{
  ept <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  expect_error(exec(reader() + summarise(), on = c(ept, ept)), "single EPT")
})

ept <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")

test_that("EPT parallel full read equals serial full read",
{
  skip_if_not(has_omp_support())
  par <- exec(reader() + summarise(), on = ept,
              ncores = concurrent_files(4))
  ser <- exec(reader() + summarise(), on = ept,
              ncores = sequential())
  expect_equal(par$npoints, ser$npoints)
  expect_equal(par$z_histogram, ser$z_histogram)
})

test_that("EPT parallel spatial query equals serial spatial query",
{
  skip_if_not(has_omp_support())
  q <- reader_rectangles(273360, 5274360, 273490, 5274490)
  par <- exec(q + summarise(), on = ept, ncores = concurrent_files(4))
  ser <- exec(q + summarise(), on = ept, ncores = sequential())
  expect_equal(par$npoints, ser$npoints)
  expect_equal(par$z_histogram, ser$z_histogram)
})

test_that("EPT partition count scales and all partitions are strict",
{
  skip_if_not(has_omp_support())
  insp <- lasR:::.APITEST$cpp_ept_partition_inspect(ept, 16)
  expect_gt(insp$nchunks, 1)
  expect_true(all(insp$strict_clip))
  expect_true(insp$tiles_built)
})

test_that("EPT partitions stay inside conforming bounds",
{
  insp <- lasR:::.APITEST$cpp_ept_partition_inspect(ept, 16)
  cb <- insp$conf_bounds
  expect_true(all(insp$bbox[,1] >= cb[1] - 1e-9))
  expect_true(all(insp$bbox[,2] >= cb[2] - 1e-9))
  expect_true(all(insp$bbox[,3] <= cb[3] + 1e-9))
  expect_true(all(insp$bbox[,4] <= cb[4] + 1e-9))
})

test_that("EPT strict-clip core ownership is unique (no duplicates, no losses)",
{
  skip_if_not(has_omp_support())
  out_dir <- file.path(tempdir(), "ept-strict-clip")
  dir.create(out_dir, showWarnings = FALSE)
  on.exit(unlink(out_dir, recursive = TRUE), add = TRUE)
  ofiles <- file.path(out_dir, "*.las")

  exec(reader() + write_las(ofiles), on = ept, ncores = concurrent_files(4))
  written <- list.files(out_dir, pattern = "\\.las$", full.names = TRUE)
  expect_gt(length(written), 1)

  total_par <- sum(sapply(written, function(f)
    exec(reader() + summarise(), on = f)$npoints))
  total_ser <- exec(reader() + summarise(), on = ept,
                    ncores = sequential())$npoints
  expect_equal(total_par, total_ser)
})

test_that("EPT explicit chunk override produces correct results",
{
  ans_chunked <- exec(reader() + summarise(), on = ept, chunk = 100,
                      ncores = sequential())
  ans_seq <- exec(reader() + summarise(), on = ept, ncores = sequential())
  # Explicit `chunk` uses non-strict clipping (overlap-based), so a few
  # boundary points may be counted in more than one chunk. The override
  # must run successfully and yield a count within a small tolerance of
  # the serial total (the auto-partitioned path uses strict clipping and
  # is exact; that case is covered by other tests).
  expect_gte(ans_chunked$npoints, ans_seq$npoints)
  expect_lt(ans_chunked$npoints, ans_seq$npoints * 1.01)
})

test_that("EPT env-var validation falls back gracefully",
{
  skip_if_not(has_omp_support())
  # The C++ layer routes warnings through REprintf rather than R's
  # warning() mechanism (project-wide convention in src/LASRcore/print.cpp),
  # so we capture stderr instead of relying on expect_warning().
  on.exit(Sys.unsetenv("LASR_EPT_PARTITIONS"), add = TRUE)
  ser_npoints <- exec(reader() + summarise(), on = ept,
                      ncores = sequential())$npoints
  for (val in c("0", "-1", "foo", "99999")) {
    Sys.setenv(LASR_EPT_PARTITIONS = val)
    msg <- capture.output(
      ans <- exec(reader() + summarise(), on = ept,
                  ncores = concurrent_files(4)),
      type = "message")
    expect_match(paste(msg, collapse = "\n"),
                 "LASR_EPT_PARTITIONS")
    expect_equal(ans$npoints, ser_npoints)
  }
})

test_that("EPT auto-partition gate matches all downgrade scenarios",
{
  gate <- lasR:::.APITEST$cpp_ept_should_auto_partition

  # Positive: EPT + parallelizable + no R callback + outer > 1
  expect_true(gate("EPTF", TRUE, FALSE, 4))

  # Downgrade A: pipeline not parallelizable
  expect_false(gate("EPTF", FALSE, FALSE, 4))

  # Downgrade B: pipeline injects R code (use_rcapi)
  expect_false(gate("EPTF", TRUE, TRUE, 4))

  # Downgrade C: only one outer thread requested
  expect_false(gate("EPTF", TRUE, FALSE, 1))

  # Format guard: non-EPT sources are not auto-partitioned
  expect_false(gate("LASF", TRUE, FALSE, 4))
  expect_false(gate("PCDF", TRUE, FALSE, 4))
})

test_that("EPT non-parallelizable pipeline still produces correct results",
{
  skip_if_not(has_omp_support())
  par <- exec(reader() + callback(function(data) data, expose = "*"),
              on = ept, ncores = concurrent_files(4))
  ser <- exec(reader() + callback(function(data) data, expose = "*"),
              on = ept, ncores = sequential())
  expect_equal(length(par), length(ser))
})

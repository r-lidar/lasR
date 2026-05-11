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

# ----- Strict-clip boundary policy (spec §5 test 5) -----
#
# Direct unit tests of the per-point ownership predicate. cpp_strict_clip_decide
# returns 0=DROP, 1=CORE, 2=BUFFERED for a single (px, py) given the chunk
# extent, buffer, and catalog global max. This catches floating-point boundary
# bugs that integration tests can't easily reach because real-world point data
# rarely lands exactly on a partition midline.

DROP <- 0L; CORE <- 1L; BUFFERED <- 2L
decide <- function(px, py, xmin, xmax, ymin, ymax,
                   buffer = 0, catalog_xmax = xmax, catalog_ymax = ymax)
{
  lasR:::.APITEST$cpp_strict_clip_decide(
    px, py, xmin, xmax, ymin, ymax, buffer,
    catalog_xmax, catalog_ymax)
}

test_that("strict-clip: core, buffer-ring, and drop regions",
{
  # Chunk [0, 10] x [0, 10] with buffer = 2, NOT at global max.
  # Catalog extends to 100, 100 so xmax/ymax carve-out won't fire.
  expect_equal(decide(5,    5,   0, 10, 0, 10, 2, 100, 100), CORE)
  expect_equal(decide(11,   5,   0, 10, 0, 10, 2, 100, 100), BUFFERED)
  expect_equal(decide(11.9, 5,   0, 10, 0, 10, 2, 100, 100), BUFFERED)
  expect_equal(decide(12.1, 5,   0, 10, 0, 10, 2, 100, 100), DROP)
  expect_equal(decide(-1,   5,   0, 10, 0, 10, 2, 100, 100), BUFFERED)
  expect_equal(decide(-2.1, 5,   0, 10, 0, 10, 2, 100, 100), DROP)
})

test_that("strict-clip: half-open ownership at the right/top edge",
{
  # px == xmax but xmax != catalog_xmax → not the rightmost cell → BUFFERED.
  expect_equal(decide(10, 5, 0, 10, 0, 10, 2, 100, 100), BUFFERED)
  expect_equal(decide(5, 10, 0, 10, 0, 10, 2, 100, 100), BUFFERED)
})

test_that("strict-clip: global-max-inclusive carve-out keeps rightmost edge",
{
  # px == xmax AND xmax == catalog_xmax → the rightmost cell owns its outer edge.
  expect_equal(decide(10, 5, 0, 10, 0, 10, 2, 10, 100), CORE)
  expect_equal(decide(5, 10, 0, 10, 0, 10, 2, 100, 10), CORE)
  expect_equal(decide(10, 10, 0, 10, 0, 10, 2, 10, 10), CORE)
})

test_that("strict-clip: half-open at xmin includes the boundary",
{
  # px == xmin → owns_x = (px >= xmin) && (px < xmax) → CORE.
  # The left/bottom edge is closed; the right/top edge is open (unless global max).
  expect_equal(decide(0, 5, 0, 10, 0, 10, 2, 100, 100), CORE)
  expect_equal(decide(5, 0, 0, 10, 0, 10, 2, 100, 100), CORE)
})

test_that("strict-clip: shared midline owned by exactly one of two adjacent cells",
{
  # Cell A:  [0, 5)  x [0, 10]      (its xmax = 5, NOT at global max)
  # Cell B:  [5, 10] x [0, 10]      (its xmin = 5)
  # Catalog xmax = 10 → only Cell B is "rightmost" in x.
  # A point at exactly x = 5 must be CORE in Cell B and BUFFERED in Cell A.
  expect_equal(decide(5, 5,   0, 5,  0, 10, 2, 10, 10), BUFFERED)
  expect_equal(decide(5, 5,   5, 10, 0, 10, 2, 10, 10), CORE)
})

test_that("strict-clip: zero buffer collapses buffer ring to nothing",
{
  # With buffer = 0, the buffered extent equals the core extent. Any point
  # strictly outside [xmin, xmax]×[ymin, ymax] is DROP, never BUFFERED.
  expect_equal(decide(5,    5,   0, 10, 0, 10, 0, 100, 100), CORE)
  expect_equal(decide(10.1, 5,   0, 10, 0, 10, 0, 100, 100), DROP)
  expect_equal(decide(5,    -0.1, 0, 10, 0, 10, 0, 100, 100), DROP)
  # px == xmax (not global) → still inside buffered extent → BUFFERED.
  expect_equal(decide(10, 5, 0, 10, 0, 10, 0, 100, 100), BUFFERED)
})

# ----- Ownership contract: get_buffered() short-circuit (spec rev 5 test 10) -----
#
# The reader flags interior-boundary points BUFFERED via strict_clip_decide,
# but summary.cpp:36 / writelas.cpp:97 historically recomputed inside_buffer()
# geometrically (inclusive >), reaching the opposite verdict at the boundary.
# These tests pin the new contract: the flag wins; geometry is the fallback.

test_that("summary honors get_buffered() flag (boundary semantics)", {
  # Flag forces "in buffer" regardless of geometry.
  expect_true(lasR:::.APITEST$cpp_summary_buffer_decide(
    TRUE,  5, 5, 0, 0, 10, 10, FALSE))
  # No flag, inside core → "not in buffer".
  expect_false(lasR:::.APITEST$cpp_summary_buffer_decide(
    FALSE, 5, 5, 0, 0, 10, 10, FALSE))
  # No flag, on the inclusive xmax edge → geometric "not in buffer".
  expect_false(lasR:::.APITEST$cpp_summary_buffer_decide(
    FALSE, 10, 5, 0, 0, 10, 10, FALSE))
  # No flag, past xmax → "in buffer" via geometry.
  expect_true(lasR:::.APITEST$cpp_summary_buffer_decide(
    FALSE, 11, 5, 0, 0, 10, 10, FALSE))
  # Motivating bug: flag set, point on xmax (non-global) → flag wins over geometry.
  expect_true(lasR:::.APITEST$cpp_summary_buffer_decide(
    TRUE,  10, 5, 0, 0, 10, 10, FALSE))
  # No flag, past ymax → "in buffer" via Y-axis geometry.
  expect_true(lasR:::.APITEST$cpp_summary_buffer_decide(
    FALSE, 5, 11, 0, 0, 10, 10, FALSE))
  # Flag set, point on ymax (non-global) → flag wins over Y-axis geometry.
  expect_true(lasR:::.APITEST$cpp_summary_buffer_decide(
    TRUE,  5, 10, 0, 0, 10, 10, FALSE))
  # No flag, circular footprint, point outside inscribed circle but inside bbox → "in buffer".
  expect_true(lasR:::.APITEST$cpp_summary_buffer_decide(
    FALSE, 9, 9, 0, 0, 10, 10, TRUE))
  # Flag set, circular footprint, point inside circle → flag wins over geometry.
  expect_true(lasR:::.APITEST$cpp_summary_buffer_decide(
    TRUE,  5, 5, 0, 0, 10, 10, TRUE))
})

test_that("writelas honors get_buffered() flag (boundary semantics)", {
  # Flag forces "drop" regardless of geometry.
  expect_false(lasR:::.APITEST$cpp_writelas_buffer_decide(
    TRUE,  5, 5, 0, 0, 10, 10, FALSE))
  # No flag, inside core → "write".
  expect_true(lasR:::.APITEST$cpp_writelas_buffer_decide(
    FALSE, 5, 5, 0, 0, 10, 10, FALSE))
  # No flag, on xmax edge → geometric "write" (inclusive).
  expect_true(lasR:::.APITEST$cpp_writelas_buffer_decide(
    FALSE, 10, 5, 0, 0, 10, 10, FALSE))
  # No flag, past xmax → "drop".
  expect_false(lasR:::.APITEST$cpp_writelas_buffer_decide(
    FALSE, 11, 5, 0, 0, 10, 10, FALSE))
  # Motivating bug: flag set, point on xmax (non-global) → flag forces drop over geometry.
  expect_false(lasR:::.APITEST$cpp_writelas_buffer_decide(
    TRUE,  10, 5, 0, 0, 10, 10, FALSE))
  # No flag, past ymax → "drop" via Y-axis geometry.
  expect_false(lasR:::.APITEST$cpp_writelas_buffer_decide(
    FALSE, 5, 11, 0, 0, 10, 10, FALSE))
  # Flag set, point on ymax (non-global) → flag forces drop over Y-axis geometry.
  expect_false(lasR:::.APITEST$cpp_writelas_buffer_decide(
    TRUE,  5, 10, 0, 0, 10, 10, FALSE))
  # No flag, circular footprint, point outside inscribed circle but inside bbox → "drop".
  expect_false(lasR:::.APITEST$cpp_writelas_buffer_decide(
    FALSE, 9, 9, 0, 0, 10, 10, TRUE))
  # Flag set, circular footprint, point inside circle → flag forces drop over geometry.
  expect_false(lasR:::.APITEST$cpp_writelas_buffer_decide(
    TRUE,  5, 5, 0, 0, 10, 10, TRUE))
})

# ----- AOI partition: passthrough cases (spec rev 5 tests 5, 6) -----

test_that("EPT partition: circle AOI passes through", {
  insp <- lasR:::.APITEST$cpp_ept_partition_inspect(
    ept, 16, list(),
    list(c(273500, 5274500, 50)))  # circle inside conf_bounds
  expect_equal(insp$nchunks, 1L)
  expect_equal(as.character(insp$shape_type), "circle")
})

test_that("EPT partition: rect AOI outside conf_bounds passes through", {
  insp <- lasR:::.APITEST$cpp_ept_partition_inspect(
    ept, 16, list(c(1, 2, 3, 4)))  # nowhere near the data
  expect_equal(insp$nchunks, 1L)
  expect_equal(as.character(insp$shape_type), "rect")
  # Note: get_chunk_with_query zeros the chunk bbox when no files overlap
  # the query — passthrough leaves the Shape* untouched, but the reported
  # chunk bbox reflects the no-file-match placeholder, not the original rect.
})

# ----- AOI partition: hierarchy walk failure (spec rev 5 test 9) -----

test_that("EPT partition: malformed sub-hierarchy with AOI → passthrough + warning", {
  src <- system.file("extdata", "ept-test-multi", package = "lasR")
  dst <- file.path(tempdir(), "ept-broken-subhier")
  unlink(dst, recursive = TRUE)
  dir.create(dst, showWarnings = FALSE)
  on.exit(unlink(dst, recursive = TRUE), add = TRUE)
  stopifnot(file.copy(file.path(src, "ept.json"), dst))
  dir.create(file.path(dst, "ept-data"), showWarnings = FALSE)
  dir.create(file.path(dst, "ept-hierarchy"), showWarnings = FALSE)
  stopifnot(all(file.copy(
    list.files(file.path(src, "ept-data"), full.names = TRUE),
    file.path(dst, "ept-data"))))
  stopifnot(all(file.copy(
    list.files(file.path(src, "ept-hierarchy"), full.names = TRUE),
    file.path(dst, "ept-hierarchy"))))

  # Engineer a -1 sub-page reference in the root hierarchy. The
  # fixture's root only contains leaves with positive counts, so we
  # add our own pointer to a sub-page that exists on disk but is
  # malformed. build_metadata returns on the first openable probe
  # (one of the existing leaves) and never visits this; ensure_tiles
  # walks the entire hierarchy and throws when it parses the malformed
  # sub-page.
  root_path <- file.path(dst, "ept-hierarchy", "0-0-0-0.json")
  root <- jsonlite::fromJSON(root_path)
  root[["2-0-0-0"]] <- -1L
  jsonlite::write_json(root, root_path, auto_unbox = TRUE)
  writeLines("{ not valid json",
             file.path(dst, "ept-hierarchy", "2-0-0-0.json"))

  endpoint <- file.path(dst, "ept.json")
  msg <- capture.output(
    insp <- lasR:::.APITEST$cpp_ept_partition_inspect(
      endpoint, 16, list(c(273400, 5274400, 273600, 5274600))),
    type = "message")
  expect_equal(insp$nchunks, 1L)
  expect_equal(as.character(insp$shape_type), "rect")
  expect_equal(insp$bbox[1, ], c(273400, 5274400, 273600, 5274600))
  expect_match(paste(msg, collapse = "\n"),
               "EPT hierarchy unavailable .* AOI partitioning skipped")
})

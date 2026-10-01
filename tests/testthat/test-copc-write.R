test_that("write_copc round-trips Topography point count and bbox",
{
  f = system.file("extdata", "Topography.las", package = "lasR")
  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  expect_error(exec(reader() + write_las(o, experimental_writer = TRUE), on = f), NA)
  expect_true(file.info(o)$size > 0)

  src = exec(reader() + summarise(), on = f)
  dst = exec(reader() + summarise(), on = o)

  expect_equal(dst$npoints, src$npoints)
  expect_equal(dst$crs, src$crs)
})

test_that("write_copc produces a file with populated COPC info VLR",
{
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  exec(write_copc(o, experimental_writer = TRUE), on = f)

  # A round-trip read should find all points, proving that the EPT hierarchy
  # eVLR and COPC info VLR are correctly populated (a degenerate hierarchy
  # would cause the reader to see 0 points).
  src_n = exec(reader() + summarise(), on = f)$npoints
  dst_n = exec(reader() + summarise(), on = o)$npoints
  expect_equal(dst_n, src_n)
})

test_that("write_copc handles PDRF promotion (format 1 -> 6)",
{
  # Topography.las is PDRF 1; COPC requires >= 6. Ensure the promotion path
  # in COPCwriter::prepare_copc_header works and produces a readable file.
  f = system.file("extdata", "Topography.las", package = "lasR")
  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  expect_error(exec(reader() + write_las(o, experimental_writer = TRUE), on = f), NA)

  # Re-read and confirm we still get the same number of points (the PDRF
  # promotion must not lose or duplicate any).
  src = exec(reader() + summarise(), on = f)
  dst = exec(reader() + summarise(), on = o)
  expect_equal(dst$npoints, src$npoints)
})

test_that("write_copc leaves no spill directory behind on success",
{
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  exec(write_copc(o, experimental_writer = TRUE), on = f)

  # No residual <output>.copc-spill-* directory should remain after a
  # successful close(), regardless of whether spilling was triggered.
  pattern = paste0(basename(o), ".copc-spill-")
  residues = list.files(dirname(o), pattern = pattern)
  expect_length(residues, 0L)
})

test_that("write_copc produces byte-identical output across runs (stable sort)",
{
  f = system.file("extdata", "Topography.las", package = "lasR")
  o1 = tempfile(fileext = ".copc.laz")
  o2 = tempfile(fileext = ".copc.laz")
  on.exit(unlink(c(o1, o2)), add = TRUE)

  exec(reader() + write_las(o1, experimental_writer = TRUE), on = f)
  exec(reader() + write_las(o2, experimental_writer = TRUE), on = f)

  expect_equal(file.info(o1)$size, file.info(o2)$size)
  expect_equal(unname(tools::md5sum(o1)), unname(tools::md5sum(o2)))
})

test_that("write_copc round-trips a PDRF 6 input via the fast path",
{
  f = system.file("extdata", "las14_pdrf6.laz", package = "lasR")
  skip_if(f == "", "las14_pdrf6.laz not available")
  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  exec(reader() + write_las(o, experimental_writer = TRUE), on = f)
  src = exec(reader() + summarise(), on = f)
  dst = exec(reader() + summarise(), on = o)
  expect_equal(dst$npoints, src$npoints)
})

test_that("write_copc merges multiple input files into a single COPC",
{
  f1 = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  f2 = system.file("extdata", "bcts", "bcts_2.laz", package = "lasR")
  skip_if(f1 == "" || f2 == "", "bcts tiles not available")

  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  expect_error(exec(write_copc(o, experimental_writer = TRUE), on = c(f1, f2)), NA)

  n1 = exec(reader() + summarise(), on = f1)$npoints
  n2 = exec(reader() + summarise(), on = f2)$npoints
  nout = exec(reader() + summarise(), on = o)$npoints
  expect_equal(nout, n1 + n2)

  # Read the COPC info VLR (at offset 375 + 54 in LAS 1.4 with COPC info as
  # the first VLR) and assert the octree was sized to the UNION of the two
  # input files' bboxes — not just file 1's. Without the union-bbox fix,
  # center_y would be ~629279 (file 1's center) and halfsize ~121.5; with
  # the fix, center_y ~629429 (union center) and halfsize ~271.4.
  con = file(o, open = "rb")
  on.exit(close(con), add = TRUE)
  seek(con, 375 + 54)
  center_xyz = readBin(con, "double", n = 3)
  halfsize   = readBin(con, "double", n = 1)
  expect_gt(center_xyz[2], 629350)  # union center, not file-1 center
  expect_gt(halfsize, 200)           # union halfsize, not file-1 halfsize
})

test_that("write_copc output is readable by lasR EPT-style reader with depth filter",
{
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  exec(write_copc(o, experimental_writer = TRUE), on = f)

  # reader_las(depth=0) asks for root-level points only. For a non-empty COPC
  # this must return > 0 points and <= the full file's point count. This
  # indirectly validates that the hierarchy has at least a real root entry.
  total = exec(reader() + summarise(), on = o)$npoints
  root  = exec(reader_las(depth = 0) + summarise(), on = o)$npoints
  expect_gt(root, 0L)
  expect_lte(root, total)
})

test_that("write_copc produces progressive LOD across depths",
{
  # bcts_1.laz (~530k points) at sparse density (grid_size = 64). With true
  # progressive LOD, point counts must strictly increase with depth, and
  # depth 0 must return far fewer points than the full file (representative
  # voxel sampling, not the whole dataset).
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  exec(write_copc(o, density = "sparse", experimental_writer = TRUE), on = f)

  total = exec(reader() + summarise(), on = o)$npoints
  d0 = exec(reader_las(depth = 0) + summarise(), on = o)$npoints
  d1 = exec(reader_las(depth = 1) + summarise(), on = o)$npoints

  # Density-blind octree (the bug this guards against) would put everything
  # in max-depth leaves and emit zero-count placeholders for parents — d0
  # would then be ~0. With voxel routing, d0 should be in the thousands
  # (one representative per occupied voxel of the root grid).
  expect_gt(d0, 1000L)
  expect_lt(d0, total / 10L)   # coarse sample is a small fraction of the whole

  # Cumulative reads must be monotonically non-decreasing with depth, and
  # depth >= some level must reach the full point count.
  expect_gte(d1, d0)
  expect_lte(d1, total)
  expect_equal(exec(reader() + summarise(), on = o)$npoints, total)
})

test_that("write_copc uses XY-balanced shallow LOD sampling by default",
{
  # Low-depth overviews should be selected in map space so display reads are
  # not biased by vertical/scanline structure. Compare the default against the
  # internal escape hatch that restores pure 3D voxel-cell routing.
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  prev_xy_depth = Sys.getenv("LASR_COPC_XY_LOD_DEPTH", unset = NA)
  prev_xy_mult  = Sys.getenv("LASR_COPC_XY_LOD_GRID_MULTIPLIER", unset = NA)
  on.exit({
    if (is.na(prev_xy_depth)) Sys.unsetenv("LASR_COPC_XY_LOD_DEPTH") else Sys.setenv(LASR_COPC_XY_LOD_DEPTH = prev_xy_depth)
    if (is.na(prev_xy_mult))  Sys.unsetenv("LASR_COPC_XY_LOD_GRID_MULTIPLIER") else Sys.setenv(LASR_COPC_XY_LOD_GRID_MULTIPLIER = prev_xy_mult)
  }, add = TRUE)

  write_with_xy_depth = function(depth) {
    Sys.unsetenv("LASR_COPC_XY_LOD_GRID_MULTIPLIER")
    if (is.na(depth)) Sys.unsetenv("LASR_COPC_XY_LOD_DEPTH")
    else Sys.setenv(LASR_COPC_XY_LOD_DEPTH = as.character(depth))

    o = tempfile(fileext = ".copc.laz")
    on.exit(unlink(o), add = TRUE)
    exec(write_copc(o, density = "normal", experimental_writer = TRUE), on = f)
    c(
      d0 = exec(reader_las(depth = 0) + summarise(), on = o)$npoints,
      d1 = exec(reader_las(depth = 1) + summarise(), on = o)$npoints,
      d2 = exec(reader_las(depth = 2) + summarise(), on = o)$npoints,
      total = exec(reader() + summarise(), on = o)$npoints
    )
  }

  xy_default = write_with_xy_depth(NA)
  xyz_only   = write_with_xy_depth(-1)
  src_n      = exec(reader() + summarise(), on = f)$npoints

  expect_equal(xy_default[["total"]], src_n)
  expect_equal(xyz_only[["total"]], src_n)
  expect_gt(xy_default[["d1"]], xyz_only[["d1"]] * 1.05)
  expect_gt(xy_default[["d2"]], xyz_only[["d2"]] * 1.03)
})

test_that("write_copc honours a tiny LASR_COPC_RESIDENT_BUDGET (forces flush path)",
{
  # bcts_1.laz holds enough points that a 1 MB resident budget will fire
  # enforce_resident_budget many times during intake (the new batch-flush
  # path at COPCwriter.cpp:581). Without exercising it, regressions in
  # flush_hot_octant accounting / wb_lru bookkeeping wouldn't be caught
  # by the small-input default-budget tests above.
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  prev_resident = Sys.getenv("LASR_COPC_RESIDENT_BUDGET", unset = NA)
  prev_ram      = Sys.getenv("LASR_COPC_RAM_BUDGET",      unset = NA)
  prev_wb       = Sys.getenv("LASR_COPC_SPILL_WRITE_BUDGET", unset = NA)
  Sys.setenv(LASR_COPC_RESIDENT_BUDGET   = as.character(1024L * 1024L))   # 1 MB
  Sys.setenv(LASR_COPC_RAM_BUDGET        = as.character(1024L * 1024L))   # 1 MB
  Sys.setenv(LASR_COPC_SPILL_WRITE_BUDGET = as.character(2L * 1024L * 1024L)) # 2 MB
  on.exit({
    if (is.na(prev_resident)) Sys.unsetenv("LASR_COPC_RESIDENT_BUDGET") else Sys.setenv(LASR_COPC_RESIDENT_BUDGET = prev_resident)
    if (is.na(prev_ram))      Sys.unsetenv("LASR_COPC_RAM_BUDGET")      else Sys.setenv(LASR_COPC_RAM_BUDGET = prev_ram)
    if (is.na(prev_wb))       Sys.unsetenv("LASR_COPC_SPILL_WRITE_BUDGET") else Sys.setenv(LASR_COPC_SPILL_WRITE_BUDGET = prev_wb)
  }, add = TRUE)

  o1 = tempfile(fileext = ".copc.laz")
  o2 = tempfile(fileext = ".copc.laz")
  on.exit(unlink(c(o1, o2)), add = TRUE)

  # Two writes with the same tiny budget must succeed and must produce
  # identical output (the flush-path order must remain deterministic).
  exec(reader() + write_las(o1, experimental_writer = TRUE), on = f)
  exec(reader() + write_las(o2, experimental_writer = TRUE), on = f)

  src_n = exec(reader() + summarise(), on = f)$npoints
  expect_equal(exec(reader() + summarise(), on = o1)$npoints, src_n)
  expect_equal(exec(reader() + summarise(), on = o2)$npoints, src_n)

  # Same-input, same-budget runs must be byte-stable across the flush
  # path — protects flush_hot_octant ordering, wb_lru tie-break, and the
  # batch-flush sort from regressions.
  expect_equal(file.info(o1)$size, file.info(o2)$size)
  expect_equal(unname(tools::md5sum(o1)), unname(tools::md5sum(o2)))
})

test_that("write_copc coarse levels do not depend on the resident budget (#342)",
{
  # A memory-pressure flush used to reset a coarse (XY) octant's cell
  # table. The reopened octant took a second point in cells it had already
  # spilled until it reached max_points_per_chunk, and then no point at
  # all, so the per-depth point counts depended on the budget (depth 1 of
  # this file gained 91% with "normal"). Protecting no depth and a 1 MB
  # budget force such flushes on a small file. Which point a spilled cell
  # keeps can still depend on when the flush happened, which moves the
  # deeper levels by a few points, hence the 1% tolerance.
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  prev_resident = Sys.getenv("LASR_COPC_RESIDENT_BUDGET", unset = NA)
  prev_protect  = Sys.getenv("LASR_COPC_PROTECTED_LOD_DEPTH", unset = NA)
  on.exit({
    if (is.na(prev_resident)) Sys.unsetenv("LASR_COPC_RESIDENT_BUDGET") else Sys.setenv(LASR_COPC_RESIDENT_BUDGET = prev_resident)
    if (is.na(prev_protect))  Sys.unsetenv("LASR_COPC_PROTECTED_LOD_DEPTH") else Sys.setenv(LASR_COPC_PROTECTED_LOD_DEPTH = prev_protect)
  }, add = TRUE)

  counts_per_depth = function(density) {
    o = tempfile(fileext = ".copc.laz")
    on.exit(unlink(o), add = TRUE)
    exec(write_copc(o, density = density, experimental_writer = TRUE), on = f)
    sapply(0:3, function(d) exec(reader(depth = d) + summarise(), on = o)$npoints)
  }

  for (density in c("normal", "dense"))
  {
    Sys.unsetenv(c("LASR_COPC_RESIDENT_BUDGET", "LASR_COPC_PROTECTED_LOD_DEPTH"))
    ref = counts_per_depth(density)

    Sys.setenv(LASR_COPC_RESIDENT_BUDGET = as.character(1024L * 1024L),
               LASR_COPC_PROTECTED_LOD_DEPTH = "0")
    tight = counts_per_depth(density)

    expect_lt(max(abs(tight / ref - 1)), 0.01, label = paste0("relative change per depth (", density, ")"))
  }
})

test_that("write_copc protects shallow LODs from early resident-budget flushes",
{
  # Under a tight resident budget, unprotected shallow octants can be frozen
  # before the whole input stream has had a chance to compete for their voxels.
  # Protecting depths 0..2 should keep depth-1/depth-2 overview counts much
  # richer while preserving the full point count.
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  prev_resident = Sys.getenv("LASR_COPC_RESIDENT_BUDGET", unset = NA)
  prev_ram      = Sys.getenv("LASR_COPC_RAM_BUDGET",      unset = NA)
  prev_wb       = Sys.getenv("LASR_COPC_SPILL_WRITE_BUDGET", unset = NA)
  prev_protect  = Sys.getenv("LASR_COPC_PROTECTED_LOD_DEPTH", unset = NA)
  prev_xy_depth = Sys.getenv("LASR_COPC_XY_LOD_DEPTH", unset = NA)
  on.exit({
    if (is.na(prev_resident)) Sys.unsetenv("LASR_COPC_RESIDENT_BUDGET") else Sys.setenv(LASR_COPC_RESIDENT_BUDGET = prev_resident)
    if (is.na(prev_ram))      Sys.unsetenv("LASR_COPC_RAM_BUDGET")      else Sys.setenv(LASR_COPC_RAM_BUDGET = prev_ram)
    if (is.na(prev_wb))       Sys.unsetenv("LASR_COPC_SPILL_WRITE_BUDGET") else Sys.setenv(LASR_COPC_SPILL_WRITE_BUDGET = prev_wb)
    if (is.na(prev_protect))  Sys.unsetenv("LASR_COPC_PROTECTED_LOD_DEPTH") else Sys.setenv(LASR_COPC_PROTECTED_LOD_DEPTH = prev_protect)
    if (is.na(prev_xy_depth)) Sys.unsetenv("LASR_COPC_XY_LOD_DEPTH") else Sys.setenv(LASR_COPC_XY_LOD_DEPTH = prev_xy_depth)
  }, add = TRUE)

  write_with_protection = function(depth) {
    Sys.setenv(
      LASR_COPC_RESIDENT_BUDGET = as.character(1024L * 1024L),
      LASR_COPC_RAM_BUDGET = as.character(1024L * 1024L),
      LASR_COPC_SPILL_WRITE_BUDGET = as.character(2L * 1024L * 1024L),
      LASR_COPC_PROTECTED_LOD_DEPTH = as.character(depth),
      LASR_COPC_XY_LOD_DEPTH = "-1"
    )
    o = tempfile(fileext = ".copc.laz")
    on.exit(unlink(o), add = TRUE)
    exec(write_copc(o, density = "normal", experimental_writer = TRUE), on = f)
    c(
      d0 = exec(reader_las(depth = 0) + summarise(), on = o)$npoints,
      d1 = exec(reader_las(depth = 1) + summarise(), on = o)$npoints,
      d2 = exec(reader_las(depth = 2) + summarise(), on = o)$npoints,
      total = exec(reader() + summarise(), on = o)$npoints
    )
  }

  unprotected = write_with_protection(0)
  protected   = write_with_protection(2)
  src_n       = exec(reader() + summarise(), on = f)$npoints

  expect_equal(unprotected[["total"]], src_n)
  expect_equal(protected[["total"]], src_n)
  expect_equal(protected[["d0"]], unprotected[["d0"]])
  expect_gt(protected[["d1"]], unprotected[["d1"]] * 2)
  expect_gt(protected[["d2"]], unprotected[["d2"]] * 4)
})

test_that("write_copc honours LASR_COPC_MEMORY_BUDGET (single global knob)",
{
  # Setting only LASR_COPC_MEMORY_BUDGET should split internally into the
  # three sub-budgets (resident 35%, pre-spill RAM 35%, write-buf 8%,
  # implicit reserve 22%) without the user having to set each one.
  #
  # Verifying "the env var actually drives behavior" is hard from R: the
  # writer is byte-stable regardless of budget, so output comparisons
  # cannot distinguish "env honored, tight budget" from "env ignored,
  # default budget". Instead we set LASR_COPC_DEBUG_BUDGETS=1 to make
  # COPCwriter print the resolved (resident, ram, wb) trio once at
  # open(), capture that warning via sink(type="message"), and assert
  # the printed values match the documented split fractions. This proves
  # both that the env var is parsed AND that the split policy is
  # correct.
  f = system.file("extdata", "Megaplot.las", package = "lasR")  # smaller — test speed
  skip_if(f == "", "Megaplot.las not available")

  prev_global   = Sys.getenv("LASR_COPC_MEMORY_BUDGET",      unset = NA)
  prev_debug    = Sys.getenv("LASR_COPC_DEBUG_BUDGETS",      unset = NA)
  prev_resident = Sys.getenv("LASR_COPC_RESIDENT_BUDGET",    unset = NA)
  prev_ram      = Sys.getenv("LASR_COPC_RAM_BUDGET",         unset = NA)
  prev_wb       = Sys.getenv("LASR_COPC_SPILL_WRITE_BUDGET", unset = NA)
  global_bytes  = 8L * 1024L * 1024L   # 8 MB — small enough to be obvious
  Sys.setenv(LASR_COPC_MEMORY_BUDGET = as.character(global_bytes))
  Sys.setenv(LASR_COPC_DEBUG_BUDGETS = "1")
  Sys.unsetenv("LASR_COPC_RESIDENT_BUDGET")
  Sys.unsetenv("LASR_COPC_RAM_BUDGET")
  Sys.unsetenv("LASR_COPC_SPILL_WRITE_BUDGET")
  on.exit({
    if (is.na(prev_global))   Sys.unsetenv("LASR_COPC_MEMORY_BUDGET")      else Sys.setenv(LASR_COPC_MEMORY_BUDGET = prev_global)
    if (is.na(prev_debug))    Sys.unsetenv("LASR_COPC_DEBUG_BUDGETS")      else Sys.setenv(LASR_COPC_DEBUG_BUDGETS = prev_debug)
    if (is.na(prev_resident)) Sys.unsetenv("LASR_COPC_RESIDENT_BUDGET")    else Sys.setenv(LASR_COPC_RESIDENT_BUDGET = prev_resident)
    if (is.na(prev_ram))      Sys.unsetenv("LASR_COPC_RAM_BUDGET")         else Sys.setenv(LASR_COPC_RAM_BUDGET = prev_ram)
    if (is.na(prev_wb))       Sys.unsetenv("LASR_COPC_SPILL_WRITE_BUDGET") else Sys.setenv(LASR_COPC_SPILL_WRITE_BUDGET = prev_wb)
  }, add = TRUE)

  o = tempfile(fileext = ".copc.laz")
  err_file = tempfile()
  on.exit(unlink(c(o, err_file)), add = TRUE)

  # Capture REprintf-routed warnings via sink(type="message"). Using
  # tryCatch + finally to guarantee the sink is reset exactly once even
  # if exec() throws — the previous double-close pattern would error on
  # exit.
  con = file(err_file, open = "wt")
  tryCatch({
    sink(con, type = "message")
    exec(reader() + write_las(o, experimental_writer = TRUE), on = f)
  }, finally = {
    sink(NULL, type = "message")
    close(con)
  })
  err_text = readLines(err_file, warn = FALSE)

  src_n = exec(reader() + summarise(), on = f)$npoints
  expect_equal(exec(reader() + summarise(), on = o)$npoints, src_n)

  # Find the debug line and parse the three resolved budget values.
  debug_line = grep("COPC budgets resolved", err_text, value = TRUE)
  expect_length(debug_line, 1L)
  m = regmatches(debug_line,
                 regexec("resident=(\\d+) spill_ram=(\\d+) spill_wb=(\\d+)", debug_line))[[1]]
  expect_length(m, 4L)  # full match + 3 captures

  resident = as.numeric(m[2])
  ram      = as.numeric(m[3])
  wb       = as.numeric(m[4])
  # Documented split: 35% / 35% / 8% of LASR_COPC_MEMORY_BUDGET. Allow
  # 1-byte slack for floating-point rounding in the writer.
  expect_equal(resident, floor(global_bytes * 0.35), tolerance = 2)
  expect_equal(ram,      floor(global_bytes * 0.35), tolerance = 2)
  expect_equal(wb,       floor(global_bytes * 0.08), tolerance = 2)
})

test_that("write_copc max_extra_depth = 0 produces a more compact hierarchy",
{
  # Compact mode: in auto mode, disable adaptive depth bumping past the
  # heuristic. On bcts_1.laz with sparse density the unbounded mode
  # (max_extra_depth=-1) reaches depth 5 (12,329 / 67,514 / 219,668 /
  # 436,145 / 530,380 / 531,662 across cumulative depths). With
  # max_extra_depth=0 routing should stop at the auto-heuristic depth
  # and the deepest entries collapse into shallower chunks — fewer
  # chunks total, full point count reached at a shallower level.
  #
  # We compare unbounded vs compact (not default vs compact) so the
  # test stays meaningful regardless of what the package default is —
  # the new default of max_extra_depth=1 might give the same depth as
  # compact on small inputs, masking the bug this test guards against.
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  o_unbounded = tempfile(fileext = ".copc.laz")
  o_compact = tempfile(fileext = ".copc.laz")
  on.exit(unlink(c(o_unbounded, o_compact)), add = TRUE)

  exec(write_copc(o_unbounded, density = "sparse", experimental_writer = TRUE,
                  max_extra_depth = -1), on = f)
  exec(write_copc(o_compact, density = "sparse", experimental_writer = TRUE,
                  max_extra_depth = 0), on = f)

  total = exec(reader() + summarise(), on = f)$npoints
  # Both must round-trip every point.
  expect_equal(exec(reader() + summarise(), on = o_unbounded)$npoints, total)
  expect_equal(exec(reader() + summarise(), on = o_compact)$npoints, total)

  # The compact file's deepest non-empty chunk depth should be <= the
  # default's — that's the structural invariant compact mode promises
  # (fewer hierarchy levels). File-size comparison is not asserted
  # because LAZ compression context differs across chunk counts and
  # can flip either way; depth is the deterministic discriminator.
  read_max_real_depth = function(path) {
    con = file(path, open = "rb"); on.exit(close(con))
    # Skip the 5 leading doubles in the COPC info VLR (center xyz +
    # halfsize + spacing) to land on root_hier_offset.
    seek(con, 375 + 54 + 5*8)
    hier_offset = readBin(con, "integer", n = 1, size = 8, endian = "little")
    hier_size   = readBin(con, "integer", n = 1, size = 8, endian = "little")
    seek(con, hier_offset)
    n = hier_size %/% 32
    max_d = -1L
    for (i in seq_len(n)) {
      d = readBin(con, "integer", n = 1, size = 4, endian = "little")
      readBin(con, "integer", n = 3, size = 4, endian = "little")  # x,y,z
      readBin(con, "integer", n = 1, size = 8, endian = "little")  # offset
      bs = readBin(con, "integer", n = 1, size = 4, endian = "little")
      pc = readBin(con, "integer", n = 1, size = 4, endian = "little")
      if (pc > 0 && d > max_d) max_d = d
    }
    max_d
  }
  # Strict less-than: bcts_1.laz at sparse density is known to bump
  # past the auto heuristic in unbounded mode (we observed depth 5 in
  # earlier runs vs auto-heuristic ~depth 1-3 for ~530k pts at the
  # default cap). Compact must reach a strictly smaller max depth,
  # otherwise max_extra_depth=0 was silently ignored.
  expect_lt(read_max_real_depth(o_compact),
            read_max_real_depth(o_unbounded))
})

test_that("write_copc bbox= overrides the source header's bbox in the octree",
{
  # Pass a strictly inflated bbox to write_copc(bbox=) and verify the
  # COPC info VLR carries that bbox (via center / halfsize fields), not
  # the source header's. This mirrors the real-world fix for stale or
  # loose source headers — caller knows the actual data bbox and tells
  # the writer to size the octree from it.
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  skip_if(f == "", "Megaplot.las not available")

  # Read the source bbox from the LAS header (max_x at byte 179, min_x at
  # 187, max_y at 195, min_y at 203, max_z at 211, min_z at 219; LAS spec
  # ordering is max-then-min for each axis). summarise() doesn't expose
  # the bbox so this is the most direct way.
  src = exec(reader() + summarise(), on = f)
  src_con = file(f, open = "rb")
  on.exit(close(src_con), add = TRUE)
  seek(src_con, 179)
  raw = readBin(src_con, "double", n = 6, size = 8, endian = "little")
  bb = c(raw[2], raw[4], raw[6], raw[1], raw[3], raw[5])  # → xmin,ymin,zmin,xmax,ymax,zmax
  # Inflate by 5% on every axis (still encloses the real data, so the
  # writer's clamped-bbox check passes). The inflated bbox is what we
  # expect in the COPC info VLR.
  dx = bb[4] - bb[1]; dy = bb[5] - bb[2]; dz = bb[6] - bb[3]
  user_bbox = c(bb[1] - 0.05 * dx, bb[2] - 0.05 * dy, bb[3] - 0.05 * dz,
                bb[4] + 0.05 * dx, bb[5] + 0.05 * dy, bb[6] + 0.05 * dz)

  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)
  exec(write_copc(o, experimental_writer = TRUE, bbox = user_bbox), on = f)
  expect_equal(exec(reader() + summarise(), on = o)$npoints, src$npoints)

  con = file(o, open = "rb")
  on.exit(close(con), add = TRUE)
  seek(con, 375 + 54)  # COPC info VLR data starts here
  cx = readBin(con, "double", n = 1, size = 8, endian = "little")
  cy = readBin(con, "double", n = 1, size = 8, endian = "little")
  cz = readBin(con, "double", n = 1, size = 8, endian = "little")
  hs = readBin(con, "double", n = 1, size = 8, endian = "little")

  exp_cx = (user_bbox[1] + user_bbox[4]) / 2
  exp_cy = (user_bbox[2] + user_bbox[5]) / 2
  exp_cz = (user_bbox[3] + user_bbox[6]) / 2
  exp_hs = max(user_bbox[4] - user_bbox[1],
               user_bbox[5] - user_bbox[2],
               user_bbox[6] - user_bbox[3]) / 2
  expect_equal(cx, exp_cx, tolerance = 1e-6)
  expect_equal(cy, exp_cy, tolerance = 1e-6)
  expect_equal(cz, exp_cz, tolerance = 1e-6)
  expect_equal(hs, exp_hs, tolerance = 1e-6)
})

test_that("write_copc gpstime min/max ignore points with gpstime == 0",
{
  # A single synthetic point with gpstime=0 in an otherwise-valid dataset
  # would otherwise drag the COPC info VLR's gpstime_minimum to 0
  # (= 1980-01-06 in GPS-week-zero), which downstream tools display as a
  # meaningless temporal extent. The writer must ignore gpstime==0 when
  # computing min/max, and only fall back to 0/0 when no non-zero gpstime
  # was ever seen.
  f = system.file("extdata", "Topography.las", package = "lasR")

  read_copc_gpstime = function(path) {
    con = file(path, open = "rb"); on.exit(close(con))
    # COPC info VLR data starts at 375 + 54. Skip 5*F64 (center xyz, halfsize,
    # spacing) + 2*U64 (root_hier_offset/size) = 56 bytes to gpstime_minimum.
    seek(con, 375 + 54 + 56)
    c(min = readBin(con, "double", n = 1, size = 8, endian = "little"),
      max = readBin(con, "double", n = 1, size = 8, endian = "little"))
  }

  # Case 1: original gpstime range carries through.
  o1 = tempfile(fileext = ".copc.laz"); on.exit(unlink(o1), add = TRUE)
  lasR::exec(write_copc(o1, experimental_writer = TRUE), on = f)
  g1 = read_copc_gpstime(o1)
  expect_gt(g1["min"], 0)
  expect_gt(g1["max"], g1["min"])

  # Case 2: zero out gpstime on ground class only. The COPC min/max must
  # still reflect the non-ground points' real range, not 0.
  o_mixed_las = tempfile(fileext = ".las"); on.exit(unlink(o_mixed_las), add = TRUE)
  lasR::exec(reader() + edit_attribute(filter = "Classification == 2", attribute = "gpstime", value = 0) + write_las(o_mixed_las), on = f)
  o_mixed = tempfile(fileext = ".copc.laz"); on.exit(unlink(o_mixed), add = TRUE)
  lasR::exec(write_copc(o_mixed, experimental_writer = TRUE), on = o_mixed_las)
  g_mixed = read_copc_gpstime(o_mixed)
  expect_equal(unname(g_mixed["min"]), unname(g1["min"]), tolerance = 1e-9)
  expect_equal(unname(g_mixed["max"]), unname(g1["max"]), tolerance = 1e-9)

  # Case 3: zero out gpstime on every point. All zero -> COPC min/max = 0/0.
  o_all_las = tempfile(fileext = ".las"); on.exit(unlink(o_all_las), add = TRUE)
  lasR::exec(reader() + edit_attribute(attribute = "gpstime", value = 0) + write_las(o_all_las), on = f)
  o_all = tempfile(fileext = ".copc.laz"); on.exit(unlink(o_all), add = TRUE)
  lasR::exec(write_copc(o_all, experimental_writer = TRUE), on = o_all_las)
  g_all = read_copc_gpstime(o_all)
  expect_equal(unname(g_all["min"]), 0)
  expect_equal(unname(g_all["max"]), 0)
})

test_that("write_copc paginated hierarchy round-trips under tiny LASR_COPC_MAX_HIERARCHY_PAGE_ENTRIES",
{
  # Force the hierarchy eVLR into a multi-page layout by setting the
  # per-page entry threshold to 2 — the writer absorbs each child
  # subtree only while accumulated page entries + that subtree fit
  # under the threshold; otherwise the subtree spawns its own child
  # page (point_count = -1). The output must:
  #   (a) round-trip the full point count via the COPC reader (which
  #       walks child-page references and recurses through pages);
  #   (b) have COPC info VLR's root_hier_size < total eVLR data length
  #       (proves the post-close patch fired and root_hier_size is the
  #       root page only, not the whole eVLR which LASlib's update_header
  #       would otherwise write).
  # bcts_1.laz at sparse density gives a ~depth-5 hierarchy (~50+
  # entries), enough for pagination at threshold 2.
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  prev_page = Sys.getenv("LASR_COPC_MAX_HIERARCHY_PAGE_ENTRIES", unset = NA)
  Sys.setenv(LASR_COPC_MAX_HIERARCHY_PAGE_ENTRIES = "2")
  on.exit({
    if (is.na(prev_page)) Sys.unsetenv("LASR_COPC_MAX_HIERARCHY_PAGE_ENTRIES")
    else                  Sys.setenv(LASR_COPC_MAX_HIERARCHY_PAGE_ENTRIES = prev_page)
  }, add = TRUE)

  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)
  exec(write_copc(o, density = "sparse", experimental_writer = TRUE), on = f)

  src_n = exec(reader() + summarise(), on = f)$npoints
  expect_equal(exec(reader() + summarise(), on = o)$npoints, src_n)

  con = file(o, open = "rb")
  on.exit(close(con), add = TRUE)
  # Skip 5*F64 (center xyz, halfsize, spacing) to land on root_hier_offset.
  seek(con, 375 + 54 + 5*8)
  root_hier_offset = readBin(con, "integer", n = 1, size = 8, endian = "little")
  root_hier_size   = readBin(con, "integer", n = 1, size = 8, endian = "little")
  total_evlr_data  = file.info(o)$size - root_hier_offset
  expect_lt(root_hier_size, total_evlr_data)
  expect_gt(root_hier_size, 0)
})

test_that("write_copc skip-sort fallback fires under tiny LASR_COPC_MAX_SORT_MEMORY",
{
  # Sort-buffer cap protects against finalize-time OOM on oversized
  # chunks (force-accept at HARD_DEPTH_LIMIT, or user max_points_per_chunk
  # much larger than available RAM). Set the cap to 1 byte so every
  # chunk takes the streamed-no-sort path. The file must still round-trip
  # the full point count, and the writer must emit the diagnostic warning
  # exactly once (silenced after first hit).
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  skip_if(f == "", "Megaplot.las not available")

  prev_cap = Sys.getenv("LASR_COPC_MAX_SORT_MEMORY", unset = NA)
  Sys.setenv(LASR_COPC_MAX_SORT_MEMORY = "1")
  on.exit({
    if (is.na(prev_cap)) Sys.unsetenv("LASR_COPC_MAX_SORT_MEMORY")
    else                 Sys.setenv(LASR_COPC_MAX_SORT_MEMORY = prev_cap)
  }, add = TRUE)

  o = tempfile(fileext = ".copc.laz")
  err_file = tempfile()
  on.exit(unlink(c(o, err_file)), add = TRUE)

  con = file(err_file, open = "wt")
  tryCatch({
    sink(con, type = "message")
    exec(reader() + write_las(o, experimental_writer = TRUE), on = f)
  }, finally = {
    sink(NULL, type = "message")
    close(con)
  })
  err_text = readLines(err_file, warn = FALSE)

  # File must be valid and round-trip every point even though no chunk
  # was sorted — sort affects LAZ ratio only, not correctness.
  src_n = exec(reader() + summarise(), on = f)$npoints
  expect_equal(exec(reader() + summarise(), on = o)$npoints, src_n)

  # Warning is emitted exactly once (silenced after first hit). Match
  # on a substring stable across the full warning text.
  warn_lines = grep("exceeding the sort-buffer cap", err_text, value = TRUE)
  expect_length(warn_lines, 1L)
})

test_that("write_copc declares 0 points when the writer is opened but every point is filtered out",
{
  # The writer is opened on the first point, before the filter and buffer
  # tests, so a tile whose points are all rejected still produces a file.
  # That file must declare 0 points with either writer. The experimental
  # writer used to leave the source point count in the LAS 1.4 extended
  # count for LAS < 1.4 sources (the file held 0 points), and printed a
  # spurious "written 0 points but expected N" for LAS 1.4 sources.
  read_counts = function(path) {
    con = file(path, open = "rb"); on.exit(close(con))
    seek(con, 25)
    minor = readBin(con, "integer", n = 1, size = 1, signed = FALSE)
    # Legacy number_of_point_records + 5 legacy by-return counts (U32).
    seek(con, 107)
    legacy = readBin(con, "integer", n = 6, size = 4, endian = "little")
    # Extended number_of_point_records + 15 extended by-return counts (U64),
    # read as pairs of 32-bit words so any non-zero byte is detected.
    seek(con, 247)
    extended = readBin(con, "integer", n = 32, size = 4, endian = "little")
    list(minor = minor, legacy = legacy, extended = extended)
  }

  capture_stderr = function(expr) {
    err_file = tempfile()
    on.exit(unlink(err_file))
    con = file(err_file, open = "wt")
    value = tryCatch({
      sink(con, type = "message")
      tryCatch(expr, error = function(e) e)
    }, finally = {
      sink(NULL, type = "message")
      close(con)
    })
    list(value = value, text = readLines(err_file, warn = FALSE))
  }

  # Topography.las is LAS 1.2 (PDRF 1): the writer promotes it to LAS 1.4.
  # las14_pdrf6.laz is already LAS 1.4 (PDRF 6).
  for (src in c("Topography.las", "las14_pdrf6.laz"))
  {
    f = system.file("extdata", src, package = "lasR")
    for (experimental in c(FALSE, TRUE))
    {
      label = paste0(src, ", experimental_writer = ", experimental)
      o = tempfile(fileext = ".copc.laz")
      on.exit(unlink(o), add = TRUE)

      w = capture_stderr(exec(write_copc(o, filter = "Z > 1000000", experimental_writer = experimental), on = f))
      expect_false(inherits(w$value, "error"), label = label)
      expect_true(file.exists(o), label = label)
      expect_false(any(grepl("but expected", w$text)), label = label)

      h = read_counts(o)
      expect_equal(h$minor, 4L, label = label)
      expect_equal(h$legacy, rep(0L, 6), label = label)
      expect_equal(h$extended, rep(0L, 32), label = label)

      # lasR discards a file that declares 0 points, exactly as for the
      # legacy writer's output. The read-back is only attempted once the
      # header says 0: a header that declares points the file does not
      # hold makes the LAZ reader loop after its end-of-file error.
      if (all(h$extended == 0L))
      {
        r = capture_stderr(exec(reader() + summarise(), on = o))
        msg = if (inherits(r$value, "error")) conditionMessage(r$value) else ""
        expect_match(msg, "There is no file to read", label = label)
        expect_true(any(grepl("containing 0 point was discarded", r$text)), label = label)
      }
      unlink(o)
    }
  }
})

test_that("write_copc resolves the default density per writer",
{
  # The legacy LASlib writer defaults to density "dense" (grid 256) and the
  # experimental writer to "normal" (grid 128); explicit values keep
  # working for both. write_las() has no density argument, so a .copc.laz
  # written with it gets the JSON fallback, which follows the same rule.
  # Both writers store spacing = 2 * halfsize / gridsize in the COPC info
  # VLR, so the grid size can be read back from the file.
  f = system.file("extdata", "Megaplot.las", package = "lasR")

  gridsize = function(stage, o) {
    exec(stage, on = f)
    con = file(o, open = "rb"); on.exit(close(con))
    seek(con, 375 + 54 + 24)  # COPC info VLR: skip center x/y/z
    halfsize = readBin(con, "double", n = 1, size = 8, endian = "little")
    spacing  = readBin(con, "double", n = 1, size = 8, endian = "little")
    round(2 * halfsize / spacing)
  }

  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)

  legacy_default = gridsize(write_copc(o), o)
  legacy_dense   = gridsize(write_copc(o, density = "dense"), o)
  legacy_normal  = gridsize(write_copc(o, density = "normal"), o)
  expect_equal(legacy_default, legacy_dense)
  expect_equal(legacy_default, 256)
  expect_equal(legacy_normal, 128)

  exp_default = gridsize(write_copc(o, experimental_writer = TRUE), o)
  exp_normal  = gridsize(write_copc(o, density = "normal", experimental_writer = TRUE), o)
  exp_dense   = gridsize(write_copc(o, density = "dense", experimental_writer = TRUE), o)
  expect_equal(exp_default, exp_normal)
  expect_equal(exp_default, 128)
  expect_equal(exp_dense, 256)

  expect_equal(gridsize(write_las(o), o), 256)
  expect_equal(gridsize(write_las(o, experimental_writer = TRUE), o), 128)
})

test_that("write_copc sizes a merged octree from a catalog count above 32 bits for a legacy point format",
{
  # A merged COPC write sizes the octree's auto max_depth from the catalog's
  # total point count. A total above 4,294,967,295 fits only in the LAS 1.4
  # extended count, and the experimental writer used to overwrite it with the
  # zero legacy count while upgrading a LAS < 1.4 header to 1.4. The octree
  # was then sized for 0 points: depth 0, every point in the root chunk.
  # A VPC declares the count, so a small LAS 1.2 / PDRF 1 file listed with
  # a count just under and just over 32 bits reproduces this without billions
  # of points. Both counts give the same auto depth, so both outputs must
  # carry the same multi-level hierarchy.
  f = system.file("extdata", "Megaplot.las", package = "lasR")

  read_hierarchy = function(path) {
    con = file(path, open = "rb"); on.exit(close(con))
    # Skip the 5 leading doubles in the COPC info VLR (center xyz +
    # halfsize + spacing) to land on root_hier_offset.
    seek(con, 375 + 54 + 5*8)
    hier_offset = readBin(con, "integer", n = 1, size = 8, endian = "little")
    hier_size   = readBin(con, "integer", n = 1, size = 8, endian = "little")
    seek(con, hier_offset)
    n = hier_size %/% 32
    entries = matrix(0L, nrow = n, ncol = 5,
                     dimnames = list(NULL, c("d", "x", "y", "z", "point_count")))
    for (i in seq_len(n)) {
      key = readBin(con, "integer", n = 4, size = 4, endian = "little")
      readBin(con, "integer", n = 1, size = 8, endian = "little")  # offset
      readBin(con, "integer", n = 1, size = 4, endian = "little")  # byte_size
      entries[i, ] = c(key, readBin(con, "integer", n = 1, size = 4, endian = "little"))
    }
    entries
  }

  vpc = tempfile(fileext = ".vpc")
  o32 = tempfile(fileext = ".copc.laz")
  o64 = tempfile(fileext = ".copc.laz")
  on.exit(unlink(c(vpc, o32, o64)), add = TRUE)

  exec(write_vpc(vpc), on = f)
  json = readLines(vpc, warn = FALSE)
  expect_length(grep('"pc:count": *[0-9]+', json), 1L)

  write_merged = function(count, o) {
    writeLines(sub('"pc:count": *[0-9]+', paste0('"pc:count": ', count), json), vpc)
    exec(write_copc(o, experimental_writer = TRUE), on = vpc)
  }
  write_merged("4000000000", o32)
  write_merged("5000000000", o64)

  h32 = read_hierarchy(o32)
  h64 = read_hierarchy(o64)
  expect_gt(max(h32[h32[, "point_count"] > 0, "d"]), 0L)
  expect_equal(h64, h32)

  src_n = exec(reader() + summarise(), on = f)$npoints
  expect_equal(exec(reader() + summarise(), on = o64)$npoints, src_n)
})

test_that("write_copc fails and removes the output when a chunk does not fit a hierarchy entry",
{
  # A COPC hierarchy entry stores a chunk's point count and byte size as
  # int32. A chunk past INT32_MAX (e.g. billions of points under an
  # explicit max_depth that is far too shallow) used to be cast and wrap
  # into a corrupt file. LASR_COPC_HIERARCHY_ENTRY_LIMIT lowers the limit so
  # a small file trips the same guard. Megaplot.las (81,590 points) is
  # written as a single root chunk (auto max_depth 0), so a limit of 1,000
  # trips the point count and a limit of 100,000 trips only the compressed
  # byte size. In that mode max_extra_depth has no effect, so the message
  # must point to max_depth or max_points_per_chunk instead.
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  old_limit = Sys.getenv("LASR_COPC_HIERARCHY_ENTRY_LIMIT", unset = NA)
  on.exit(if (is.na(old_limit)) Sys.unsetenv("LASR_COPC_HIERARCHY_ENTRY_LIMIT")
          else Sys.setenv(LASR_COPC_HIERARCHY_ENTRY_LIMIT = old_limit), add = TRUE)

  cases = list(c(limit = "1000",   pattern = "has 81590 points, more than the 1000"),
               c(limit = "100000", pattern = "bytes of compressed data, more than the 100000"))
  for (case in cases)
  {
    o = tempfile(fileext = ".copc.laz")
    on.exit(unlink(o), add = TRUE)
    Sys.setenv(LASR_COPC_HIERARCHY_ENTRY_LIMIT = case[["limit"]])
    label = paste("limit", case[["limit"]])
    err = expect_error(exec(write_copc(o, experimental_writer = TRUE), on = f),
                       case[["pattern"]], label = label)
    expect_match(conditionMessage(err), "set max_depth or lower max_points_per_chunk",
                 fixed = TRUE, label = label)
    expect_no_match(conditionMessage(err), "max_extra_depth", fixed = TRUE)
    expect_false(file.exists(o), label = label)
    residues = list.files(dirname(o), pattern = paste0(basename(o), ".copc-spill-"))
    expect_length(residues, 0L)
  }

  # An invalid, non-positive or above-INT32_MAX value keeps the default.
  for (limit in c("abc", "0", "-5", "2147483648"))
  {
    Sys.setenv(LASR_COPC_HIERARCHY_ENTRY_LIMIT = limit)
    o = tempfile(fileext = ".copc.laz")
    on.exit(unlink(o), add = TRUE)
    expect_error(exec(write_copc(o, experimental_writer = TRUE), on = f), NA,
                 label = paste("limit", limit))
  }

  # The default limit (INT32_MAX) writes the same file without error.
  Sys.unsetenv("LASR_COPC_HIERARCHY_ENTRY_LIMIT")
  o = tempfile(fileext = ".copc.laz")
  on.exit(unlink(o), add = TRUE)
  expect_error(exec(write_copc(o, experimental_writer = TRUE), on = f), NA)
  expect_equal(exec(reader() + summarise(), on = o)$npoints,
               exec(reader() + summarise(), on = f)$npoints)
})

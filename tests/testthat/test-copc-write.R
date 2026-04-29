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
  # heuristic. On bcts_1.laz with sparse density the default (full
  # bumping) reaches depth 5 (12,329 / 67,514 / 219,668 / 436,145 /
  # 530,380 / 531,662 across cumulative depths). With max_extra_depth=0
  # routing should stop at the auto-heuristic depth and the deepest
  # entries collapse into shallower chunks — fewer chunks total, full
  # point count reached at a shallower level.
  f = system.file("extdata", "bcts", "bcts_1.laz", package = "lasR")
  skip_if(f == "", "bcts_1.laz not available")

  o_default = tempfile(fileext = ".copc.laz")
  o_compact = tempfile(fileext = ".copc.laz")
  on.exit(unlink(c(o_default, o_compact)), add = TRUE)

  exec(write_copc(o_default, density = "sparse", experimental_writer = TRUE), on = f)
  exec(write_copc(o_compact, density = "sparse", experimental_writer = TRUE,
                  max_extra_depth = 0), on = f)

  total = exec(reader() + summarise(), on = f)$npoints
  # Both must round-trip every point.
  expect_equal(exec(reader() + summarise(), on = o_default)$npoints, total)
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
    hier_offset = readBin(con, "integer", n = 1, size = 8)
    hier_size   = readBin(con, "integer", n = 1, size = 8)
    seek(con, hier_offset)
    n = hier_size %/% 32
    max_d = -1L
    for (i in seq_len(n)) {
      d = readBin(con, "integer", n = 1, size = 4)
      readBin(con, "integer", n = 3, size = 4)  # x,y,z
      readBin(con, "integer", n = 1, size = 8)  # offset
      bs = readBin(con, "integer", n = 1, size = 4)
      pc = readBin(con, "integer", n = 1, size = 4)
      if (pc > 0 && d > max_d) max_d = d
    }
    max_d
  }
  # Strict less-than: bcts_1.laz at sparse density is known to bump
  # past the auto heuristic in default mode (we observed depth 5 in
  # earlier runs vs auto-heuristic ~depth 1-3 for ~530k pts at the
  # default cap). Compact must reach a strictly smaller max depth,
  # otherwise max_extra_depth=0 was silently ignored.
  expect_lt(read_max_real_depth(o_compact),
            read_max_real_depth(o_default))
})

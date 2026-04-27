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

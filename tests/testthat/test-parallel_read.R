# The global strategy set in setup.R would override the 'ncores' given to exec(): unset it in this file
strategy = get_parallel_strategy()
unset_parallel_strategy()

read_all = function(on, ncores, filter = "", ...)
{
  pipe = reader(filter = filter) + callback(function(d) list(d), expose = "*", drop_buffer = FALSE, no_las_update = TRUE)
  exec(pipe, on = on, ncores = ncores, ...)
}

laz = local({
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  o = tempfile(fileext = ".laz")
  exec(write_las(o), on = f)
  o
})

test_that("parallel decoding returns the same points in the same order (LAZ)",
{
  a = read_all(laz, sequential())
  b = read_all(laz, concurrent_points(2))
  expect_identical(a, b)
  expect_equal(nrow(a[[1]]), 81590L)
})

test_that("parallel decoding returns the same points in the same order (LAS with LAX)",
{
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  expect_identical(read_all(f, sequential()), read_all(f, concurrent_points(3)))
})

test_that("parallel decoding applies LASlib filters in order",
{
  expect_identical(read_all(laz, sequential(), "-keep_class 2"), read_all(laz, concurrent_points(2), "-keep_class 2"))
  expect_identical(read_all(laz, sequential(), "-keep_every_nth 7"), read_all(laz, concurrent_points(2), "-keep_every_nth 7"))
})

test_that("LASlib transforms are read sequentially",
{
  # A transform may depend on the order of the points: no parallel decoding
  expect_identical(read_all(laz, sequential(), "-translate_z 10"), read_all(laz, concurrent_points(2), "-translate_z 10"))
  expect_identical(read_all(laz, sequential(), "-keep_class 2 -scale_z 2"), read_all(laz, concurrent_points(2), "-keep_class 2 -scale_z 2"))
})

test_that("parallel decoding works with several files and a buffer",
{
  d = tempfile()
  dir.create(d)
  exec(write_las(file.path(d, "*.laz")), on = laz, chunk = 60)
  f = list.files(d, full.names = TRUE)

  expect_identical(read_all(f, sequential()), read_all(f, concurrent_points(2)))
  expect_identical(read_all(f, sequential(), buffer = 5), read_all(f, concurrent_points(2), buffer = 5))
  expect_identical(read_all(f, concurrent_files(2), buffer = 5), read_all(f, concurrent_points(2), buffer = 5))
})

read_with = function(on, ncores, rd, ...)
{
  pipe = rd + callback(function(d) list(d), expose = "*", drop_buffer = FALSE, no_las_update = TRUE)
  exec(pipe, on = on, ncores = ncores, ...)
}

decoding = function(on, rd, ...)
{
  pipe = rd + nothing(read = TRUE)
  x = capture.output(exec(pipe, on = on, ncores = concurrent_points(2), verbose = TRUE, ...))
  unique(grep("Decoding", x, value = TRUE))
}

test_that("parallel decoding reads only the intervals of a LAX index, in the same order",
{
  d = tempfile()
  dir.create(d)
  exec(write_las(file.path(d, "*.laz")), on = laz, chunk = 80)
  f = list.files(d, pattern = "laz$", full.names = TRUE)
  exec(write_lax(), on = f)
  expect_true(all(file.exists(sub("laz$", "lax", f))))

  # Buffered chunks: the neighbour files are partly in the query
  expect_identical(read_with(f, sequential(), reader(), buffer = 10), read_with(f, concurrent_points(2), reader(), buffer = 10))
  expect_true(all(grepl("Decoding with 2 threads", decoding(f, reader(), buffer = 10))))

  # Chunks smaller than the files: a file is both a main file and a neighbour file
  expect_identical(read_with(f, sequential(), reader(), chunk = 50, buffer = 5), read_with(f, concurrent_points(2), reader(), chunk = 50, buffer = 5))

  # Circles and an order dependent filter
  xc = c(684800, 684900, 684950)
  yc = c(5017800, 5017900, 5017990)
  expect_identical(read_with(f, sequential(), reader_circles(xc, yc, 20)), read_with(f, concurrent_points(2), reader_circles(xc, yc, 20)))
  expect_identical(read_with(f, sequential(), reader(filter = "-keep_every_nth 3"), buffer = 10), read_with(f, concurrent_points(2), reader(filter = "-keep_every_nth 3"), buffer = 10))
})

test_that("parallel decoding reads only the intervals of a COPC index, in the same order",
{
  f = system.file("extdata", "Megaplot.las", package = "lasR")
  copc = tempfile(fileext = ".copc.laz")
  exec(write_copc(copc, max_depth = 3, density = "sparse"), on = f)

  # lasR's COPC writer stores a chunk size of 0 (variable chunks for LASzip): it must be decoded in parallel
  expect_true(all(grepl("Decoding with 2 threads", decoding(copc, reader(depth = 1)))))

  for (depth in list(NULL, 0, 1, 2))
    expect_identical(read_with(copc, sequential(), reader(depth = depth)), read_with(copc, concurrent_points(2), reader(depth = depth)))

  xc = c(684800, 684900, 684950)
  yc = c(5017800, 5017900, 5017990)
  expect_identical(read_with(copc, sequential(), reader_circles(xc, yc, 20)), read_with(copc, concurrent_points(2), reader_circles(xc, yc, 20)))
  expect_identical(read_with(copc, sequential(), reader(depth = 1), chunk = 100, buffer = 10), read_with(copc, concurrent_points(2), reader(depth = 1), chunk = 100, buffer = 10))
})

test_that("a truncated file is an R error, not a crash, with every strategy",
{
  f = tempfile(fileext = ".laz")
  file.copy(laz, f)
  size = file.size(laz)
  con = file(f, "r+b")
  bytes = readBin(con, "raw", n = size)
  close(con)
  writeBin(bytes[seq_len(round(size * 0.7))], f)

  pipe = reader() + nothing(read = TRUE)
  expect_error(suppressWarnings(exec(pipe, on = f, ncores = sequential())))
  expect_error(suppressWarnings(exec(pipe, on = f, ncores = concurrent_points(2))))
  expect_error(suppressWarnings(exec(pipe, on = c(f, laz), ncores = concurrent_files(2))))
})

test_that("the reader uses threads with concurrent-points only",
{
  pipe = reader() + nothing(read = TRUE)
  x = capture.output(exec(pipe, on = laz, ncores = concurrent_points(2), verbose = TRUE))
  expect_true(any(grepl("Decoding with 2 threads", x)))

  x = capture.output(exec(pipe, on = laz, ncores = sequential(), verbose = TRUE))
  expect_false(any(grepl("Decoding with", x)))
})

# Restore the strategy of setup.R
set_parallel_strategy(strategy)

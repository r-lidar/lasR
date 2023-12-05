f = paste0(system.file(package="lasR"), "/extdata/bcts/")
f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

read_las = function(f, select = "xyzib", filter = "")
{
  load = function(data) { return(data) }
  call = callback(load, expose = select, filter, no_las_update = TRUE)
  call
}

test_that("reader_circle perform a queries",
{
  pipeline = reader_circles(f, 885100, 629300, 10) + read_las()
  ans = processor(pipeline)
  expect_equal(dim(ans), c(3469L, 5L))

  # between two tiles
  pipeline = reader_circles(f, 885150, 629400, 10) + read_las()
  ans = processor(pipeline)
  expect_equal(dim(ans), c(6368, 5L))

  # between two tiles with a buffer
  pipeline = reader_circles(f, 885150, 629400, 10, buffer = 5L) + read_las()
  ans = processor(pipeline)
  expect_equal(dim(ans), c(14074, 5L))
  expect_equal(sum(ans$Buffer), 7706L)

  # between two tiles with a buffer but the centroid is not in a file
  pipeline = reader_rectangles(f, 885000L, 629390, 885040, 629410, buffer = 5) + read_las()
  ans = processor(pipeline)
  expect_equal(dim(ans), c(5028L, 5L))
  expect_equal(sum(ans$Buffer), 2415L)

  # no match
  pipeline = reader_circles(f, 8850000, 629400, 20, buffer = 5) + read_las()
  expect_error(processor(pipeline), "cannot find")
})

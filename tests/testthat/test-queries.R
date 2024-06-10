f = paste0(system.file(package="lasR"), "/extdata/bcts/")
f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

read = function()
{
  load = function(data) { return(data) }
  call = callback(load, expose = "xyzrib", no_las_update = TRUE)
  call
}

test_that("reader_circle perform a queries",
{
  pipeline = reader_las_circles(885100, 629300, 10, filter = keep_first()) + read()
  ans = exec(pipeline, f)
  expect_equal(dim(ans), c(2665L, 6L))
  expect_equal(table(ans$ReturnNumber), c(`1` = 2665L), ignore_attr = T)

  # between two tiles
  pipeline = reader_las_circles(885150, 629400, 10, filter = keep_ground()) + read()
  ans = exec(pipeline, f)
  expect_equal(dim(ans), c(314L, 6L))

  # between two tiles with a buffer
  pipeline = reader_las_circles(885150, 629400, 10) + read()
  ans = exec(pipeline, f, buffer = 5)

  expect_equal(dim(ans), c(14074, 6L))
  expect_equal(sum(ans$Buffer), 7706L)

  # between two tiles with a buffer
  pipeline = reader_las_circles(885150, 629400, 10, filter = keep_first()) + read()
  ans = exec(pipeline, f, buffer = 5)

  expect_equal(dim(ans), c(10263, 6L))
  expect_equal(sum(ans$Buffer), 5691L)

  # between two tiles with a buffer but the centroid is not in a file
  pipeline = reader_las_rectangles(885000L, 629390, 885040, 629410) + read()
  ans = exec(pipeline, f, buffer = 5)
  expect_equal(dim(ans), c(5028L, 6L))
  expect_equal(sum(ans$Buffer), 2415L)

  # no match
  pipeline = reader_las_circles(8850000, 629400, 20) + read()
  expect_error(exec(pipeline, f, buffer = 5), "cannot find")
})

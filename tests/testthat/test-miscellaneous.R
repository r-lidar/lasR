test_that("miscellaneous",
{
  dtm = triangulate() + hulls()

  expect_error(transform_with(dtm), "has 2 stages")
  expect_error(transform_with(10), "The stage stage must be a 'PipelinePtr'")
})

test_that("temp files",
{
  expect_error(temptif(), NA)
  expect_error(tempgpkg(), NA)
  expect_error(tempshp(), NA)
  expect_error(templas(), NA)
  expect_error(templaz(), NA)
})

test_that("empty folder #160",
{
  dir = tempfile()
  dir.create(dir)
  expect_error(lasR::exec(lasR:::nothing(), on = dir), "There is no file to read")
})

test_that(".onLoad",
{
  expect_error(lasR:::.onLoad(), NA)
  set_parallel_strategy(concurrent_files(2))
})
test_that("miscellaneous",
{
  dtm = triangulate() + hulls()

  expect_error(transform_with(dtm), "has 2 stages")
  expect_error(transform_with(10), "The stage stage must be a 'LASRalgorithm'")
})

test_that("temp files",
{
  expect_error(temptif(), NA)
  expect_error(tempgpkg(), NA)
  expect_error(tempshp(), NA)
  expect_error(templas(), NA)
  expect_error(templaz(), NA)
})


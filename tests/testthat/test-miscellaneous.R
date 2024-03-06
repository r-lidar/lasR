test_that("miscellaneous",
{
  dtm = triangulate() + hulls()

  expect_error(transform_with(dtm), "has 2 stages")
  expect_error(transform_with(10), "The stage stage must be a 'LASRalgorithm'")
})

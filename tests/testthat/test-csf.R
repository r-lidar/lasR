test_that("CSF works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  pipeline = classify_with_csf(TRUE, 1 ,1, time_step = 1, filter = "") + summarise()
  ans = exec(pipeline, on = f)
  ans$npoints_per_class

  class = c(226,45282,23998,3897)
  names(class) = c(0,1,2,9)
  expect_equal(ans$npoints_per_class, class, tolerance = 1)
})

test_that("CSF works with a filter",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  pipeline = classify_with_csf(TRUE, 1 ,1, time_step = 1) + summarise()
  ans = exec(pipeline, on = f)
  ans$npoints_per_class

  class = c(175, 46509, 22822, 3897)
  names(class) = c(0,1,2,9)
  expect_equal(ans$npoints_per_class, class, tolerance = 1)
})

test_that("CSF does not read past the ground index vector (#321)",
{
  # Regression test for #321: once all ground indices have been matched, the
  # classification loop used to evaluate ground[k] with k == ground.size() for
  # every remaining (non-ground) point, an out-of-bounds heap read. Point clouds
  # almost always end on non-ground points, so the read happened on nearly every
  # call. The run must complete and conserve the total point count; under
  # AddressSanitizer/valgrind this also catches the out-of-bounds access itself.
  f <- system.file("extdata", "Topography.las", package="lasR")

  pipeline = classify_with_csf(TRUE, 1, 1, time_step = 1, filter = "") + summarise()
  ans = exec(pipeline, on = f)

  expect_true(sum(ans$npoints_per_class) == ans$npoints)
  expect_true(ans$npoints_per_class[["2"]] > 0)
})

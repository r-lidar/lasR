# ===== IVF ====

test_that("classify noise with ivf works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_ivf(n = 50)
  ans = exec(class+summarise(), f)

  expect_equal(ans$npoints_per_class, c(`1` = 60721, `2` = 8053, `9` = 3835, `18` = 794))
})

test_that("classify noise with ivf works with non cubic voxels",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_ivf(res = c(5,5,10), n = 50)
  ans = exec(class+summarise(), f)

  expect_equal(ans$npoints_per_class, c(`1` = 61256, `2` = 8099, `9` = 3830, `18` = 218))

  expect_error(classify_with_ivf(res = c(5,5), n = 50), "res must be of length 3")
})


test_that("classify noise with ivf works using unorder map",
{
  skip("Not testable with new API")

  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_ivf(n = 50)
  ans = exec(class+summarise(), f)
  class$classify_with_ivf$force_map = TRUE

  expect_equal(ans$npoints_per_class, c(`1` = 60721, `2` = 8053, `9` = 3835, `18` = 794))
})

#  ===== SOR =====

test_that("classify noise with sor works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_sor(m = 3)
  ans = exec(class+summarise(), f)

  expect_equal(ans$npoints_per_class, c(`1` = 60563, `2` = 8076, `9` = 3877, `18` = 887))
  expect_equal(sum(ans$npoints_per_class), 73403L)

  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_sor(k = 5, m = 6)
  ans = exec(class+summarise(), f)

  expect_equal(ans$npoints_per_class, c(`1` = 61301, `2` = 8158, `9` = 3896, `18` = 48))
  expect_equal(sum(ans$npoints_per_class), 73403L)
})

test_that("classify noise with sor fails if k < 2",
{
  expect_error(classify_with_sor(k = 1), "less than 2-nearest neighbors")
})

# ===== IPF ====

test_that("classify noise with ipf works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_ipf(r = 4, n = 0L)
  ans = exec(class+summarise(), f)

  expect_equal(ans$npoints_per_class, c(`1` = 60267, `2` = 7954, `9` = 3889, `18` = 1293))

  class = classify_with_ipf(r = 4, n = 1L)
  ans = exec(class+summarise(), f)

  expect_equal(ans$npoints_per_class, c(`1` = 56907, `2` = 7365, `9` = 3860, `18` = 5271))
})


test_that("classify noise with ipf fails if r < 0",
{
  expect_error(classify_with_ipf(r = -1), "radius must be positive")
})



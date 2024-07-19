test_that("classify noise with ivf works",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_ivf(n = 50)
  ans = exec(class+summarise(), f)

  expect_equal(ans$npoints_per_class, c(`1` = 60721, `2` = 8053, `9` = 3835, `18` = 794))
})

test_that("classify noise with ivf works using unorder map",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_ivf(n = 50)
  ans = exec(class+summarise(), f)
  class$classify_with_ivf$force_map = TRUE

  expect_equal(ans$npoints_per_class, c(`1` = 60721, `2` = 8053, `9` = 3835, `18` = 794))
})

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
  f <- system.file("extdata", "Topography.las", package="lasR")
  class = classify_with_sor(k = 1)
  expect_error(exec(class, f), "less than 2-nearest neighbors")
})


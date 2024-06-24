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

test_that("hulls works with multiple files",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  bound = lasR::hulls()
  ans = exec(bound, f)
  ans

  expect_s3_class(ans, "sf")
  expect_equal(dim(ans), c(4L,1L))
  expect_equal(length(ans$geom[[1]]), 1L)
})


test_that("hulls works with triangulation",
{
  f <- system.file("extdata", "Topography.las", package="lasR")
  del = triangulate(15, filter = keep_ground())
  bound = lasR::hulls(del)
  ans = exec(del+bound, f)
  ans

  expect_s3_class(ans, "sf")
  expect_equal(dim(ans), c(1L,1L))
  expect_equal(length(ans$geom[[1]]), 5L) # there are outer and inner rings
})

test_that("hulls works with complex shapes",
{
  skip("Complex shapes for triangulation hulls not supported")

  f <- system.file("extdata", "Megaplot.las", package="lasR")
  del = triangulate(15, filter = keep_z_above(20), ofile = tempgpkg())
  bound = lasR::hulls(del)
  ans = exec(del+bound, f)

  plot(ans$triangulate)
  plot(ans$hulls)

  f <- system.file("extdata", "Megaplot.las", package="lasR")
  del = triangulate(-15, filter = keep_z_above(20), ofile = tempgpkg())
  bound = lasR::hulls(del)
  ans = exec(del+bound, f)

  plot(ans$triangulate)
  plot(ans$hulls)
})


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
  expect_true(sf::st_is_valid(ans))
  expect_equal(dim(ans), c(1L,1L))

  out = sf::st_exterior_ring(ans)

  area1 = as.numeric(sf::st_area(ans))
  area2 = as.numeric(sf::st_area(out))

  expect_equal(area1, 65657.9700)
  expect_equal(area2, 76306.8242)
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

test_that("hulls works with multiple file and a buffer",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  f = list.files(f, full.names = TRUE, pattern = "\\.laz")

  read = reader_las()
  bound = lasR::hulls()
  ans1 = exec(bound, f)
  ans2 = exec(bound, f, buffer = 10)

  expect_s3_class(ans2, "sf")
  expect_equal(dim(ans2), c(4L,1L))
  expect_equal(sum(sf::st_area(ans1)),   sum(sf::st_area(ans2)))
})


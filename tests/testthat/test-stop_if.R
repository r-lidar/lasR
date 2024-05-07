test_that("stop_if works",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  read = reader_las()
  max1 = rasterize(10, "max", ofile = temptif())
  max2 = rasterize(10, "max", ofile = temptif())
  stopif = lasR:::stop_if_outside(884800, 620000, 885400, 629200)
  hll1 = hulls(ofile = tempgpkg())
  hll2 = hulls(ofile = tempgpkg())
  no = lasR:::nothing(TRUE)

  t0 = Sys.time()
  pipeline = read + max1 + stopif + hll1 + no
  ans1 = exec(pipeline, on = f)
  t1 = Sys.time()
  dt1 = difftime(t1, t0, units = "secs")

  t0 = Sys.time()
  pipeline = stopif + read + max2 + hll2 + no
  ans2 <- exec(pipeline, on = f)
  t1 = Sys.time()
  dt2 = difftime(t1, t0, units = "secs")

  expect_equal(sum(is.na(ans1$rasterize[])), 100L)
  expect_equal(sum(is.na(ans2$rasterize[])), 1780L)
  expect_equal(nrow(ans1$hulls), 1L)
  expect_equal(nrow(ans2$hulls), 1L)
  expect_equal(as.numeric(sf::st_bbox(ans1$hulls)), c(885022.37, 629157.18, 885210.15, 629399.99))
  expect_equal(as.numeric(sf::st_bbox(ans2$hulls)), c(885022.37, 629157.18, 885210.15, 629399.99))
  expect_lt(dt2,dt1)
})

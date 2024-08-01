test_that("focal works",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")

  r0 = rasterize(2, "zmax")
  r1 = lasR:::focal(r0, 8, fun = "mean")
  r2 = lasR:::focal(r0, 8, fun = "max")
  r3 = lasR:::focal(r0, 8, fun = "median")
  r4 = lasR:::focal(r0, 8, fun = "min")
  r5 = lasR:::focal(r0, 8, fun = "sum")
  pipeline <- reader_las()+r0+r1+r2+r3+r4+r5
  u = exec(pipeline, on = f)

  rr1 = u[[1]][]
  rr2 = u[[2]][]
  rr3 = u[[3]][]
  rr4 = u[[4]][]
  rr5 = u[[5]][]
  rr6 = u[[6]][]

  #terra::plot(u[[1]])
  expect_equal(sum(is.na(rr1)), 3554)
  expect_equal(mean(rr1, na.rm = TRUE), 810.33647)

  #terra::plot(u[[2]])
  expect_equal(sum(is.na(rr2)), 1608)
  expect_equal(mean(rr2, na.rm = TRUE), 809.98175)

  #terra::plot(u[[3]])
  expect_equal(sum(is.na(rr3)), 1608)
  expect_equal(mean(rr3, na.rm = TRUE), 814.49932)

  #terra::plot(u[[4]])
  expect_equal(sum(is.na(rr4)), 1608)
  expect_equal(mean(rr4, na.rm = TRUE), 809.89811)

  #terra::plot(u[[5]])
  expect_equal(sum(is.na(rr5)), 1608)
  expect_equal(mean(rr5, na.rm = TRUE), 805.87213)

  #terra::plot(u[[6]])
  expect_equal(sum(is.na(rr6)), 1608)
  expect_equal(mean(rr6, na.rm = TRUE), 9369.75)
})

test_that("focal works with multibands",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")

  r0 = rasterize(2, c("zmax", "zmin"))
  r1 = lasR:::focal(r0, 8, fun = "mean")

  pipeline <- reader_las()+r0+r1
  u = exec(pipeline, on = f)

  #terra::plot(u[[1]])
  #terra::plot(u[[2]])

  expect_equal(sum(is.na(u[[2]][])), 1608*2)
  expect_equal(mean(u[[2]][][,1], na.rm = TRUE), 809.98175)
  expect_equal(mean(u[[2]][][,2], na.rm = TRUE), 806.23164)
})

test_that("focal works with 1 pixel",
{
  f <- system.file("extdata", "Topography.las", package = "lasR")

  r0 = rasterize(2, "zmax")
  r1 = lasR:::focal(r0, 1, fun = "mean")

  pipeline <- reader_las()+r0+r1
  u = exec(pipeline, on = f)

  #terra::plot(u[[1]])
  #terra::plot(u[[2]])

  expect_true(all.equal(u[[1]][], u[[2]][]))
})

test_that("remove_attribute works", {
  f = system.file("extdata", "Example.las", package="lasR")
  p = remove_attribute("Classification") + write_pcd(tempfile(fileext = ".pcd"), binary = FALSE)
  ans = exec(p, on = f)

 res1 = read_las(f)
 res2 = read.table(ans, skip = 11)
 names(res2) = c("x", "y", "z", "Intensity", "ReturnNumber", "NumberOfReturns", "UserData", "PointSourceID", "ScanAngle", "gpstime")

 expect_equal(nrow(res1), nrow(res2))
 expect_equal(res1$UserData, res2$UserData)
 expect_equal(res1$gpstime, res2$gpstime)
 expect_equal(res1$Z, res2$z)
})

test_that("remove_attributes works", {
  f = system.file("extdata", "Example.las", package="lasR")
  p = remove_attributes(c("Intensity", "Classification",  "NumberOfReturns", "UserData")) + write_pcd(tempfile(fileext = ".pcd"), binary = FALSE)
  ans = exec(p, on = f)

  res1 = read_las(f)
  res2 = read.table(ans, skip = 11)
  names(res2) = c("x", "y", "z", "ReturnNumber", "PointSourceID", "ScanAngle", "gpstime")

  expect_equal(nrow(res1), nrow(res2))
  expect_equal(res1$PointSourceID, res2$PointSourceID)
  expect_equal(res1$gpstime, res2$gpstime)
  expect_equal(res1$Z, res2$z)
})


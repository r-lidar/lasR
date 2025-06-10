
infile <- system.file("extdata", "Example.las", package="lasR")
outfile <- tempfile(fileext = ".pcd")

test_that("Write PCD works binary",
{
  ans = exec(write_pcd(ofile = outfile), on = infile)
  ouf = read_las(outfile)
  inf = read_las(infile)

  expect_equal(inf$X, ouf$X)
  expect_equal(inf$Y, ouf$Y)
  expect_equal(inf$Z, ouf$Z)
  expect_equal(inf$gpstime, ouf$gpstime)
  expect_equal(inf$Intensity, ouf$Intensity)
  expect_equal(inf$NumberOfReturns, ouf$NumberOfReturns)
  expect_equal(inf$Classification, ouf$Classification)
  expect_equal(inf$ScanAngle, ouf$ScanAngle)
  expect_equal(inf$UserData, ouf$UserData)
})

test_that("Write PCD works ascii",
{
  ans = exec(write_pcd(ofile = outfile, binary = FALSE), on = infile)

  inf = read_las(infile)

  ouf = read_las(outfile)

  out = read.table(outfile, skip = 11)

  expect_equal(inf$X, out[,1])
  expect_equal(inf$Y, out[,2])
  expect_equal(inf$Z, out[,3])
  expect_equal(inf$Intensity, out[,4])
  expect_equal(inf$ReturnNumber, out[,5])
  expect_equal(inf$gpstime, out[,11])

  expect_equal(inf$X, ouf$X)
  expect_equal(inf$Y, ouf$Y)
  expect_equal(inf$Z, ouf$Z)
  expect_equal(inf$gpstime, ouf$gpstime)
  expect_equal(inf$Intensity, ouf$Intensity)
  expect_equal(inf$NumberOfReturns, ouf$NumberOfReturns)
  expect_equal(inf$Classification, ouf$Classification)
  expect_equal(inf$ScanAngle, ouf$ScanAngle)
  expect_equal(inf$UserData, ouf$UserData)
})

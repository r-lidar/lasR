test_that("lasR supports a LAS from lidR",
{
  skip_if_not_installed("lidR")

  LASfile <- system.file("extdata", "Megaplot.las", package="lasR")
  las = eval(parse(text = "lidR::readLAS(LASfile)")) # eval(parse()) tricks R CMD check because lidR is not a dependency

  r1 = reader(las, filter = drop_z_below(2))
  r2 = reader(LASfile, filter = drop_z_below(2))

  tri = triangulate(25)
  h = hulls(tri)
  pipeline1 = r1  + tri + h
  pipeline2 = r2  + tri + h

  ans1 = processor(pipeline1)
  ans2 = processor(pipeline2)

  expect_s3_class(ans1, "sf")
  expect_equal(nrow(ans1$geom[[1]][[1]]), nrow(ans2$geom[[1]][[1]]))
})

test_that("lasR supports a LAScatalog from lidR",
{
  skip("C stack usage  is too close to the limit")
  skip_if_not_installed("lidR")

  LASfile <- system.file("extdata", "Example.las", package="lasR")
  las = eval(parse(text = "lidR::readLAScatalog(LASfile)")) # eval(parse()) tricks R CMD check because lidR is not a dependency

  pipeline = reader(las) + hulls()

  expect_error(processor(pipeline), NA)
})

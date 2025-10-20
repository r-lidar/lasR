test_that("lasR supports a LAS from lidR",
{
  skip_if_not_installed("lidR")

  LASfile <- system.file("extdata", "Megaplot.las", package="lasR")
  las = eval(parse(text = "lidR::readLAS(LASfile)")) # eval(parse()) tricks R CMD check because lidR is not a dependency

  r1 = reader_las(filter = drop_z_below(2))
  r2 = reader_las(filter = drop_z_below(2))

  tri = triangulate(25)
  h = hulls(tri)
  pipeline1 = r1  + tri + h
  pipeline2 = r2  + tri + h

  ans1 = exec(pipeline1, on = las)
  ans2 = exec(pipeline2, on = LASfile)

  expect_s3_class(ans1, "sf")
  expect_true(sf::st_is_valid(ans1))
  expect_true(sf::st_is_valid(ans2))

  area1 = as.numeric(sf::st_area(ans1))
  area2 = as.numeric(sf::st_area(ans2))

  expect_equal(area1, 44531.4675)
  expect_equal(area2, 44531.4675)

  pipeline3 = r2  + tri + h + reader_las()
  expect_error(exec(pipeline3, on = LASfile), "The pipeline can only have a single reader stage")
})

test_that("lasR supports a LAScatalog from lidR",
{
  skip_if_not_installed("lidR")

  LASfile <- system.file("extdata", "Example.las", package="lasR")
  las = eval(parse(text = "lidR::readLAScatalog(LASfile)")) # eval(parse()) tricks R CMD check because lidR is not a dependency
  las@processing_options$progress = FALSE

  pipeline = hulls()

  expect_error(exec(pipeline, on = las), NA)
})

test_that("geometry_features works",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- geometry_features(k = 8, features = "*") + write_las()
  ans <- exec(pipeline, on = f)
  las = read_las(ans)

  attr = c("coeff00", "coeff01", "coeff02", "coeff10", "coeff11","coeff12","coeff20", "coeff21","coeff22",
            "lambda1", "lambda2", "lambda3",
            "anisotropy", "planarity", "sphericity", "linearity", "omnivariance", "curvature", "eigensum", "angle",
            "normalX", "normalY", "normalZ")

  expect_true(all(attr %in% names(las)))
  expect_equal(mean(las$coeff00), 0.306148, tolerance = 1e-3)
  expect_equal(mean(las$coeff01), -0.1654238, tolerance = 1e-3)
  expect_equal(mean(las$coeff02), -0.04826465, tolerance = 1e-3)
  expect_equal(mean(las$lambda1), 1.0659, tolerance = 1e-3)
  expect_equal(mean(las$anisotropy), 0.9724, tolerance = 1e-3)
  expect_equal(mean(las$angle), 80.6690, tolerance = 1e-3)
})

test_that("geometry_features overwrites",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- geometry_features(k = 8, features = "*") + geometry_features(k = 10, features = "i") + write_las()
  ans <- exec(pipeline, on = f)
  las = read_las(ans)

  attr = c("coeff00", "coeff01", "coeff02", "coeff10", "coeff11","coeff12","coeff20", "coeff21","coeff22",
           "lambda1", "lambda2", "lambda3",
           "anisotropy", "planarity", "sphericity", "linearity", "omnivariance", "curvature", "eigensum", "angle",
           "normalX", "normalY", "normalZ")

  expect_true(all(attr %in% names(las)))
  expect_equal(mean(las$coeff00), 0.306148, tolerance = 1e-3)
  expect_equal(mean(las$coeff01), -0.1654238, tolerance = 1e-3)
  expect_equal(mean(las$coeff02), -0.04826465, tolerance = 1e-3)
  expect_equal(mean(las$lambda1), 1.0659, tolerance = 1e-3)
  expect_equal(mean(las$anisotropy), 0.9724, tolerance = 1e-3)
  expect_equal(mean(las$angle), 83.6690, tolerance = 1e-3)
})

test_that("geometry_features overwrites",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- add_extrabytes("int", "angle", "desc")  + geometry_features(k = 10, features = "i")
  expect_error(exec(pipeline, on = f), "Cannot add a second attribute 'angle'")
})


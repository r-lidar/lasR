test_that("geometry_features works",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- geometry_features(k = 8, features = "*") + write_las()
  ans <- exec(pipeline, on = f)
  las = read_las(ans)

  attr = c("coeff00", "coeff01", "coeff02", "coeff10", "coeff11","coeff12","coeff20", "coeff21","coeff22",
            "lambda1", "lambda2", "lambda3",
            "anisotropy", "planarity", "sphericity", "linearity", "omnivariance", "curvature", "eigensum", "inclination",
            "normalX", "normalY", "normalZ")

  expect_true(all(attr %in% names(las)))
  expect_equal(mean(las$coeff00), 0.306148, tolerance = 1e-3)
  expect_equal(mean(las$coeff01), -0.1654238, tolerance = 1e-3)
  expect_equal(mean(las$coeff02), -0.04826465, tolerance = 1e-3)
  expect_equal(mean(las$lambda1), 1.0659, tolerance = 1e-3)
  expect_equal(mean(las$anisotropy), 0.9724, tolerance = 1e-3)
  expect_equal(mean(las$inclination), 80.6690, tolerance = 1e-3)
  expect_equal(mean(las$normalX), -0.04826, tolerance = 1e-3)
  expect_equal(mean(las$normalZ), 0.13028, tolerance = 1e-3)
})

test_that("geometry_features overwrites",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- geometry_features(k = 8, features = "*") + geometry_features(k = 10, features = "i") + write_las()
  ans <- exec(pipeline, on = f)
  las = read_las(ans)

  attr = c("coeff00", "coeff01", "coeff02", "coeff10", "coeff11","coeff12","coeff20", "coeff21","coeff22",
           "lambda1", "lambda2", "lambda3",
           "anisotropy", "planarity", "sphericity", "linearity", "omnivariance", "curvature", "eigensum", "inclination",
           "normalX", "normalY", "normalZ")

  expect_true(all(attr %in% names(las)))
  expect_equal(mean(las$coeff00), 0.306148, tolerance = 1e-3)
  expect_equal(mean(las$coeff01), -0.1654238, tolerance = 1e-3)
  expect_equal(mean(las$coeff02), -0.04826465, tolerance = 1e-3)
  expect_equal(mean(las$lambda1), 1.0659, tolerance = 1e-3)
  expect_equal(mean(las$anisotropy), 0.9724, tolerance = 1e-3)
  expect_equal(mean(las$inclination), 83.6690, tolerance = 1e-3)
})

test_that("geometry_features works with radius",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- geometry_features(k = 8, r = 0.01, features = "E") + write_las()
  ans <- exec(pipeline, on = f)
  las = read_las(ans)

  expect_equal(sum(is.nan(las$lambda1)), 24)
})

test_that("geometry_features overwrites",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- add_extrabytes("int", "inclination", "desc")  + geometry_features(k = 10, features = "i")
  expect_error(exec(pipeline, on = f), "Cannot add a second attribute 'inclination'")
})

test_that("knn with deleted points does not crash",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  pipeline = delete_points("Classification == 2") + geometry_features(k = 15 , features = "lps")
  expect_error(exec(pipeline, on = f), NA)
})

test_that("geometry_features normals always up",
{
  f <- system.file("extdata", "Example.las", package = "lasR")
  pipeline <- geometry_features(k = 8, features = "n", always_up = TRUE) + write_las()
  ans <- exec(pipeline, on = f)
  las = read_las(ans)

  expect_true(all(las$normalZ > 0))
  expect_equal(mean(las$normalZ), 0.161260, tolerance = 1e-3)
})


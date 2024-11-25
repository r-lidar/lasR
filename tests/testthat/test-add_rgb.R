test_that("add rgb works",
{
  f <- system.file("extdata", "Example.las", package="lasR")

  pipeline <- add_rgb() + write_las()
  ans = exec(pipeline, on = f)

  las = read_las(ans)

  expect_true(all(c("R", "G", "B") %in% names(las)))
  expect_equal(sum(las$R), 0L)
})

test_that("add rgb works with extra bytes",
{
  f <- system.file("extdata", "extra_byte.las", package="lasR")

  pipeline <- add_rgb() + write_las()
  ans = exec(pipeline, on = f)

  las = read_las(ans)

  expect_true(all(c("Amplitude", "Pulse width", "R", "G", "B") %in% names(las)))
  expect_equal(sum(las$R), 0L)
  expect_equal(mean(las$gpstime), 152900)
  expect_equal(sum(las$Amplitude), 596.24)
  expect_equal(sum(las$Intensity), 6552)

})

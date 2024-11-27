test_that("metric_engine works",
{
  f <- system.file("extdata", "Example.las", package="lasR")
  p = summarise(metrics = c("z_max", "i_min", "r_mean", "n_median", "z_sd", "c_sd", "t_cv", "u_sum", "p_mode", "a_mean", "count", "z_p95", "R_sum", "B_mean", "z_above975"))
  ans = exec(p, on = f, noread = T)
  m = ans$metrics

  expect_equal(m$z_max, 978.345, tolerance = 1e-6)
  expect_equal(m$i_min, 27)
  expect_equal(m$r_mean, 1.133333, tolerance = 1e-6)
  expect_equal(m$z_sd, 1.459199, tolerance = 1e-6)
  expect_equal(m$c_sd, 0.3051286, tolerance = 1e-6)
  expect_equal(m$t_cv, 4.46e-7, tolerance = 1e-6)
  expect_equal(m$u_sum, 960)
  expect_equal(m$p_mode, 17)
  expect_equal(m$a_mean, -21.66666, tolerance = 1e-6)
  expect_equal(m$count, 30)
  expect_equal(m$z_p95, 978.2654, tolerance = 1e-6)
  expect_equal(m$R_sum, 0)
  expect_equal(m$z_above975, 0.633333, tolerance = 1e-6)
})

test_that("metric_engine works with extrabyte",
{

  f <- system.file("extdata", "extra_byte.las", package="lasR")
  p = summarise(metrics = c("Amplitude_mean", "Pulse width_max"))
  ans = exec(p, on = f, noread = T)
  m = ans$metrics

  expect_equal(m$Amplitude_mean, 9.616775, tolerance = 1e-6)
  expect_equal(m$`Pulse width_max`, 8.4, tolerance = 1e-6)
})

test_that("metric_engine works with non existing extrabyte",
{
  f <- system.file("extdata", "Example.las", package="lasR")
  p = summarise(metrics = c("plop_sum", "plop_sd"))
  ans = exec(p, on = f, noread = T)
  m = ans$metrics

  expect_true(m$plop_sum, 0)
  expect_true(m$plop_sd, 0)
})

test_that("metric_engine with summarise with multiple file",
{
  # I'm sure it does not work. It must be fixed.
  expect_true(FALSE)
})

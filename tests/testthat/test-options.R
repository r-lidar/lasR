test_that("Options are working",
{
  f <- system.file("extdata", "Example.las", package="lasR")
  pipeline = lasR:::nothing()

  set_exec_options(buffer = 10, chunk = 10, progress = FALSE, ncores = 1L, verbose = FALSE, noread = FALSE)
  with = list(buffer = 10, chunk = 10, progress = FALSE, ncores = 1L, verbose = FALSE, noread = FALSE)
  expect_error(suppressWarnings(exec(pipeline, on = f, with = with, buffer = 10, chunk = 10, progress = FALSE, ncores = 1L, verbose = FALSE, noread = FALSE)), NA)

  unset_exec_option()
  unset_parallel_strategy()
  if (has_omp_support()) set_parallel_strategy(concurrent_files(2L))

})

test_that("parallel stategy tools are working",
{
  if (has_omp_support())
  {
    x = get_parallel_strategy()
    y = 2L ; attr(y, "strategy") = "concurrent-files"
    expect_equal(x, y)

    unset_parallel_strategy()
    set_parallel_strategy(concurrent_points(2L))
  }
  else
  {
    x = get_parallel_strategy()
    expect_true(is.null(x))

    unset_parallel_strategy()
  }

  # This is true with or without open mp support. Set parallel strategy check if it is true or not

  x = concurrent_points(2L)
  y = 2L ; attr(y, "strategy") = "concurrent-points"
  expect_equal(x, y)

  x = concurrent_files(2L)
  y = 2L ; attr(y, "strategy") = "concurrent-files"
  expect_equal(x, y)

  x = sequential()
  y = 1L ; attr(y, "strategy") = "sequential"
  expect_equal(x, y)

  x = nested(2,2)
  y = c(2L, 2L) ; attr(y, "strategy") = "nested"
  expect_equal(x, y)
})

test_that("set_strategy works",
{
  skip_if_not(has_omp_support())

  strategy = get_parallel_strategy()
  expect_equal(strategy, concurrent_files(2))

  unset_parallel_strategy()
  strategy = get_parallel_strategy()
  expect_true(is.null(strategy))
})

test_that("thread tools work",
{
  expect_error(half_cores(), NA)
  expect_error(ncores(), NA)

  if (!has_omp_support())
    expect_warning(set_parallel_strategy(4))
})

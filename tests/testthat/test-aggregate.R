ferr = function(x) { stop("explicit error"); }

gerr = function(x) { if (mean(x) < 339008) return(list(0, 0)) else return(0) }

herr = function(x){return(c(0L, 0L))}

test_that("aggreate handles explicit errors",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f)
  agg = lasR:::aggregate(5, ferr(Intensity))

  expect_error(processor(read + agg), "explicit error")
})

test_that("aggreate handles inconsistancies",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f, filter = "")
  agg = lasR:::aggregate(0.5, gerr(X))

  expect_error(processor(read + agg), "inconsistant number of items")
})

test_that("aggreate handles incorect format",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f, filter = "")
  agg = lasR:::aggregate(5, herr(Intensity))

  expect_error(processor(read + agg), "atomic number")
})

test_that("aggreate finds attributes",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f, filter = "")
  agg = lasR:::aggregate(5, mean(Intensity))

  expect_error(processor(read + agg), NA)
})

test_that("aggreate catches params",
{
  f = system.file("extdata", "Example.las", package="lasR")

  fun = function(x, a) { mean(x) ; return(a) }

  b = 5
  read = reader(f, filter = "")
  agg = lasR:::aggregate(5, fun(Intensity, a = b))
  ans = processor(read + agg)

  expect_true(all(ans[] == 5))
})

test_that("aggreate names each band",
{
  f = system.file("extdata", "Example.las", package="lasR")

  fun = function(x) { list(avg = mean(x), max = max(x)) }

  read = reader(f, filter = "")
  agg = lasR:::aggregate(2, fun(Intensity))
  ans = processor(read + agg)

  expect_true(all(names(ans) == c("avg", "max")))
})



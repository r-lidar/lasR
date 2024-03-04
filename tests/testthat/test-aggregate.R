ferr = function(x) { stop("explicit error"); }
gerr = function(x) { if (mean(x) < 339008) return(list(0, 0)) else return(0) }
ger2 = function(x) { if (mean(x) > 339008) return(list(0, 0)) else return(0) }
herr = function(x){return(c(a = 0L, b = 0L))}

test_that("aggregate handles explicit errors",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f)
  agg = lasR:::aggregate(5, ferr(Intensity))

  expect_error(processor(read + agg), "explicit error")
})

test_that("aggregate handles inconsistancies",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f)
  agg  = lasR:::aggregate(0.5, gerr(X))
  agg2 = lasR:::aggregate(0.5, ger2(X))

  expect_error(processor(read + agg), "inconsistant number of items")
  expect_error(processor(read + agg2), "inconsistant number of items") # test that it does no depends on unordered_map order.
})

test_that("aggregate works with a vector instead of a list",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f)
  agg = lasR:::aggregate(5, herr(Intensity))
  ans = processor(read + agg)

  expect_equal(names(ans), c("a", "b"))
  expect_s4_class(ans, "SpatRaster")
})

test_that("aggregate fails with unknown type",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f)
  agg = lasR:::aggregate(5, function(x) { return(2i+3) })

  expect_error(processor(read + agg), "numbers")
})

test_that("aggregate finds attributes",
{
  f = system.file("extdata", "Example.las", package="lasR")

  read = reader(f)
  agg = lasR:::aggregate(5, mean(Intensity))

  expect_error(processor(read + agg), NA)
})

test_that("aggregate catches params",
{
  f = system.file("extdata", "Example.las", package="lasR")

  fun = function(x, a) { mean(x) ; return(a) }

  b = 5
  read = reader(f)
  agg = lasR:::aggregate(5, fun(Intensity, a = b))
  ans = processor(read + agg)

  expect_true(all(ans[] == 5))
})

test_that("aggregate names each band",
{
  f = system.file("extdata", "Example.las", package="lasR")

  fun = function(x) { list(avg = mean(x), max = max(x)) }

  read = reader(f)
  agg = lasR:::aggregate(2, fun(Intensity))
  ans = processor(read + agg)

  expect_true(all(names(ans) == c("avg", "max")))
})


test_that("aggregate works with multiple filea",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  f = list.files(f, full.names = TRUE, pattern = "\\.laz")
  f = f[1:2]

  read = reader(f, filter = "-keep_every_nth 10")
  agg = lasR:::aggregate(10, list(iz = mean(Z)))
  ans = processor(read + agg)
  ans = ans*1

  #terra::plot(ans, col = lidR::height.colors(25))

  expect_s4_class(ans, "SpatRaster")
  expect_equal(terra::res(ans), c(10L,10L))
  expect_equal(dim(ans), c(55L,20L,1L))
  expect_equal(names(ans), "iz")
  expect_equal(mean(ans[], na.rm = TRUE), 334.119, tolerance = 1e-6)
  expect_equal(sum(is.na(ans[])), 33L)
  expect_equal(unname(unlist(ans[c(10L,500L,980L)])), c(334.2441, 329.6076, NA_real_),  tolerance = 1e-6)
})


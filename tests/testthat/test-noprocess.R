test_that("it is possible to flag some files for not being process",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  noprocess = c(FALSE, FALSE, TRUE, FALSE)

  expect_error(reader(f, noprocess = noprocess), "have different length")

  f = list.files(f, full.names = TRUE, pattern = "\\.laz")

  read = reader(f, noprocess = noprocess)
  bound = lasR::hulls()
  ans = processor(read+bound)

  expect_s3_class(ans, "sf")
  expect_equal(dim(ans), c(3L,1L))
})

test_that("the file not process is used as buffer",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  f = list.files(f, full.names = TRUE, pattern = "\\.laz")[1:2]
  noprocess = c(FALSE, TRUE)

  fun = function(data) { length(data$X) }
  summ = lasR::callback(fun, expose = "x")

  ans1 = processor(reader(f[1])+summ)

  read = reader(f, noprocess = noprocess)
  ans2 = processor(read+summ)

  read = reader(f, noprocess = noprocess, buffer = 50)
  ans3 = processor(read+summ)

  expect_equal(ans1, 531662L)
  expect_equal(ans1, ans2)
  expect_equal(ans3, 682031L)
})
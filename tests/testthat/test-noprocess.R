test_that("it is possible to flag some files for not being processed",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  noprocess = c(FALSE, FALSE, TRUE, FALSE)

  read = reader_las()

  expect_error(exec(read, f, noprocess = noprocess), "have different length")

  f = list.files(f, full.names = TRUE, pattern = "\\.laz")

  bound = lasR::hulls()
  ans = exec(bound, f, noprocess = noprocess)

  expect_s3_class(ans, "sf")
  expect_equal(dim(ans), c(3L,1L))
})

test_that("the file not processed is used as buffer",
{
  f <- system.file("extdata", "bcts/", package="lasR")
  f = list.files(f, full.names = TRUE, pattern = "\\.laz")[1:2]
  noprocess = c(FALSE, TRUE)

  fun = function(data) { length(data$X) }
  summ = lasR::callback(fun, expose = "x")

  ans1 = exec(summ, f[1])
  ans2 = exec(summ, f, noprocess = noprocess)
  ans3 = exec(summ, f, noprocess = noprocess, buffer = 50)

  expect_equal(ans1, 531662L)
  expect_equal(ans1, ans2)
  expect_equal(ans3, 682031L)
})
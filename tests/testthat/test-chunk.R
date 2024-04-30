test_that("processing by chunks works",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  p = reader_las() + rasterize(20, "zmean") + local_maximum(3) + write_las()
  ans1 = exec(p, on = f, chunk = 100)
  ans2 = exec(p, on = f, chunk = 0)

  expect_equal(ans1$rasterize[], ans2$rasterize[])
  expect_equal(dim(ans1$local_maximum), c(297, 1))
  expect_equal(nrow(ans1$local_maximum), nrow(ans2$local_maximum))
  expect_equal(basename(ans1$write_las), paste0("MixedConifer_", 0:3, ".las"))

  f <- system.file("extdata", "Megaplot.las", package="lasR")
  p = reader_las() + rasterize(20, "count")
  ans1 = exec(p, on = f, chunk = 200)
  ans2 = exec(p, on = f, chunk = 0)

  expect_equal(ans1[], ans2[])
})
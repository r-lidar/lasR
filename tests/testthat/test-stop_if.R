test_that("stop_if works",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  read = reader_las()
  stopif = lasR:::stop_if_outside(884800, 620000, 885400, 629200)
  hll = hulls()
  no = lasR:::nothing(TRUE)

  pipeline = read + stopif + hll + no
  exec(pipeline, on = f)

  pipeline = stopif + read + hll + no
  exec(pipeline, on = f)
})

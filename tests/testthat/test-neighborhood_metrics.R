test_that("neighborhood metrics works",
{
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  read <- reader_las()
  lmf <- local_maximum(5)
  nnm = lasR:::neighborhood_metrics(lmf, k = 10, metrics = c("i_mean", "count"))
  ans <- exec(read + lmf + nnm, on = f)

  lm = ans$local_maximum
  nn = ans$neighborhood_metrics

  expect_equal(sf::st_coordinates(lm), sf::st_coordinates(nn))
  expect_equal(names(nn), c("i_mean", "count", "geom"))
})

test_that("buffer tiles",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  read = reader(f, buffer = 25)
  write = write_las(paste0(tempdir(), "/*_buffered.las"), keep_buffer = TRUE)
  ans = processor(read+write)

  cont1 = processor(reader(f) + boundaries())
  cont2 = processor(reader(ans) + boundaries())

  expect_equal(dim(cont1), c(4L,1L))

  # Not using sf::st_area that fails on github action because of missing crs data.base
  area = function(m) { diff(m[1:2,1]) * diff(m[2:3,2])}
  area1 = 0; for (i in 1:4) area1 = area1 + area(cont1$geom[[i]][[1]])
  area2 = 0; for (i in 1:4) area2 = area2 + area(cont2$geom[[i]][[1]])

  expect_equal(area1, 206788.110)
  expect_equal(area2, 236796.375)
})

test_that("buffer tiles",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  read = reader(f, buffer = 25)
  write = write_las(paste0(tempdir(), "/*_buffered.las"), keep_buffer = TRUE)
  ans = processor(read+write)

  cont1 = processor(reader(f) + boundaries())
  cont2 = processor(reader(ans) + boundaries())

  expect_equal(as.numeric(sum(sf::st_area(cont1))), 206788.110)
  expect_equal(as.numeric(sum(sf::st_area(cont2))), 236796.375)
})

f <- system.file("extdata", "Topography.las", package="lasR")

test_that("test1 works",
{
  res = lasR:::.APITEST$cpp_test1(f)
  expect_length(res, 3)
  expect_true(all.equal(names(res), c("reader_las", "filter", "write_las")))
  expect_true(file.exists(res$write_las))
  file.remove(res$write_las)
})


test_that("test2 works",
{
  res = lasR:::.APITEST$cpp_test2(f)
  expect_length(res, 6)
  expect_true(all.equal(names(res), c("reader_las", "sor", "filter", "rasterize", "triangulate", "rasterize")))
  expect_true(file.exists(res[[4]]))
  expect_true(file.exists(res[[6]]))

  dsm = terra::rast(res[[4]])
  dtm = terra::rast(res[[6]])

  file.remove(res[[4]])
  file.remove(res[[6]])
})

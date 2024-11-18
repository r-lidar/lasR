test_that("R convient filters work", {
  v = lasR:::validate_filter
  expect_equal(as.character(v(keep_class(c(5,7)))), "-lasr_class_in 5 7")
  expect_equal(as.character(v(drop_class(c(5,7)))), "-lasr_class_out 5 7")
  expect_equal(as.character(v(keep_first())), "-lasr_return_equal 1")
  expect_equal(as.character(v(drop_first())), "-lasr_return_different 1")
  expect_equal(as.character(v(keep_ground())), "-lasr_class_equal 2")
  expect_equal(as.character(v(drop_ground())), "-lasr_class_different 2")
  expect_equal(as.character(v(keep_noise())), "-lasr_class_equal 18")
  expect_equal(as.character(v(drop_noise())), "-lasr_class_different 18")
  expect_equal(as.character(v(keep_z_above(7))), "-lasr_z_aboveeq 7")
  expect_equal(as.character(v(keep_z_below(7))), "-lasr_z_below 7")
  expect_equal(as.character(v(drop_z_above(7))), "-lasr_z_below 7")
  expect_equal(as.character(v(drop_z_below(7))), "-lasr_z_aboveeq 7")
  expect_equal(as.character(v(drop_duplicates())), "-drop_duplicates")
  expect_equal(as.character(v(keep_ground()+keep_z_above(7))), c("-lasr_class_equal 2",  "-lasr_z_aboveeq 7"))
  expect_error(keep_ground()+ "-keep_class 2")
})

test_that("Usage works", {
  sink(tempfile())
  expect_error(filter_usage(), NA)
  expect_error(lasR:::transform_usage(), NA)
  expect_error(lasR:::available_threads(), NA)
  print(keep_first())
  sink(NULL)
})

test_that("Various filter tests",
{
  test = function(filter)
  {
    f <- system.file("extdata", "Example.las", package = "lasR")
    exec(reader_las(filter) + summarise(), on = f)
  }

  expect_equal(test("Z > 976")$npoints, 12L)
  expect_equal(test("Classification == 2")$npoints, 3L)
  expect_equal(test("Intensity < 107")$npoints, 27L)
  expect_equal(test("Intensity <= 107")$npoints, 28L)
  expect_equal(test("UserData == 32")$npoints, 30L)
  expect_equal(test("ReturnNumber != 1")$npoints, 4L)
  expect_equal(test("gpstime %between% 269347.4 269347.6")$npoints, 17L)
  expect_equal(test("z %between% 974 975")$npoints, 10L)
  expect_equal(test("Intensity %in% 107 113")$npoints, 2L)
  expect_equal(test("Intensity %out% 107 113")$npoints, 28L)
})

test_that("Various filter tests with extra attributes",
{
  test = function(filter)
  {
    f <- system.file("extdata", "extra_byte.las", package = "lasR")
    exec(reader_las(filter) + summarise(), on = f)
  }

  expect_equal(test("Amplitude < 10")$npoints, 27L)
  expect_equal(test("Amplitude == 8.27")$npoints, 2L)
  expect_equal(test("Amplitude %between% 10 8")$npoints, 8L)
  expect_equal(test("Plop > 0")$npoints, 0L)
})

test_that("Filters errors",
{
  test = function(filter)
  {
    f <- system.file("extdata", "Example.las", package = "lasR")
    exec(reader_las(filter) + summarise(), on = f)
  }

  expect_error(test("Z > 976 546")$npoints)
  expect_error(test("3 == 4")$npoints)
  #expect_error(test("-keep_z_above 234 345"))
  expect_error(test(paste0("UserData %in% ", paste(1:65, collapse = " "))))
})

test_that("rasterize works with a filter (#29)",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  imean1 <- rasterize(4, "i_mean", filter = drop_ground())
  imean2 <- rasterize(4, list(i_mean = mean(Intensity)), filter = drop_ground())
  pipeline = imean1+imean2
  ans = exec(pipeline, on = f)

  #terra::plot(ans$rasterize)
  #terra::plot(ans$aggregate)

  expect_equal(mean(ans$rasterize[], na.rm = T), 872.9656, tolerance = 1e-6)
  expect_equal(sum(is.na(ans$rasterize[])), 612)
  expect_equal(ans$rasterize[], ans$aggregate[])
})

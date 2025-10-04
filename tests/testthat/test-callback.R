convert_intensity_in_range = function(data, min, max)
{
  i = data$Intensity
  i = ((i - min(i)) / (max(i) - min(i))) * (max - min) + min
  i[i < min] = min
  i[i > max] = max
  data$Intensity = as.integer(i)
  w = logical(nrow(data))
  suppressWarnings(w[] <- c(TRUE, FALSE))
  data$Withheld = w
  return(data)
}

test_stop = function(data)
{
  stop("error in test_stop")
}

f = system.file("extdata", "Example.las", package="lasR")

test_that("callback creates and exposes data.frame with bbox",
{
  las = read_las(f, expose = "xyzi")

  expect_s3_class(las, "data.frame")
  expect_equal(dim(las), c(30L,4L))
  expect_equal(names(las), c("X", "Y", "Z", "Intensity"))
  expect_equal(attr(las, "bbox"), c(339002.9, 5248000.0,  339015.1, 5248001.2), tolerance = 0.01)
})

test_that("callback edits and deletes the points",
{
  read = reader_las()
  call = callback(convert_intensity_in_range, expose = "xyzi", min = 0, max = 255)
  write = write_las()
  pipeline = read + call + write
  ans = exec(pipeline, on = f)

  las = read_las(ans)

  expect_s3_class(las, "data.frame")
  expect_equal(range(las$Intensity), c(11,223))
  expect_equal(nrow(las), 15L, tolerance = 1)
})

test_that("callback cannot add an attribute not compatble with the format.",
{
  fun = function(data) { data$R = 0L ; data }
  read = reader_las()
  call = callback(fun, expose = "xyzi")
  pipeline = read + call
  expect_error(exec(pipeline, on = f), "non supported column 'R'")
})

test_that("callback fails if an attribute is of incorrect type.",
{
  skip("No longer valid")

  fun = function(data) { data$Classification = 0 ; data }
  read = reader_las()
  call = callback(fun, expose = "xyzi")
  pipeline = read + call
  expect_error(exec(pipeline, on = f), "Classification is expected to be integer")
})

test_that("callback without ouput and multiple file",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  read = reader_las(filter = "-keep_every_nth 10")
  call = callback(function(data) { data }, expose = "xyzi", no_las_update = TRUE)
  pipeline = read + call
  ans = exec(pipeline, on = f, buffer = 50)

  expect_type(ans, "list")
  expect_length(ans, 2)
  expect_equal(attr(ans[[1]], "bbox"), c(885022.4, 629157.2, 885210.2, 629400.0), tolerance = 0.00001)
  expect_equal(attr(ans[[2]], "bbox"), c(885024.1, 629400.0, 885217.1, 629700.0), tolerance = 0.00001)
  expect_equal(range(ans[[1]]$Y)[2], 629400.0 + 50, tolerance = 1e-7)
  expect_equal(range(ans[[2]]$Y)[1], 629400.0 - 50, tolerance = 1e-7)
  expect_equal(nrow(ans[[1]]), 68205L)
  expect_equal(nrow(ans[[2]]), 93158L)
})

test_that("callback can return any object",
{
  meanz = function(data){ return(mean(data$Z)) }
  read = reader_las()
  call = callback(meanz, expose = "xyz")
  ans = exec(read+call, on = f)

  expect_equal(ans, 975.9, tolerance = 0.01)
})

test_that("callback works if all the points are not exposed",
{
  f = system.file("extdata", "Megaplot.las", package="lasR")

  load = function(data) { return(data) }
  call1 = callback(load, no_las_update = TRUE)
  call2 = callback(load, no_las_update = TRUE, drop_buffer = TRUE)

  x = 684850
  y = 5017900
  r = 10
  read = reader_las_circles(xc = x, yc = y, r = r)
  pipeline = read + call1 + call2

  ans = exec(pipeline, on = f, buffer = 10)
  las1 = ans[[1]]
  las2 = ans[[2]]

  expect_s3_class(las1, "data.frame")
  expect_s3_class(las2, "data.frame")
  expect_equal(dim(las1), c(2151L, 3L))
  expect_equal(dim(las2), c(579, 3L))
  expect_equal(diff(range(las1$X)), 40, tolerance = 1)
  expect_equal(diff(range(las2$X)), 20, tolerance = 1)
})

f = system.file("extdata", "extra_byte.las", package="lasR")

test_that("callback exposes extrabytes",
{
  las = read_las(f, expose = "xyz01")

  expect_equal(range(las$Amplitude), c(0.58, 16.04))
  expect_equal(range(las$`Pulse width`), c(4.0, 8.4))
})

test_that("callback exposes everything",
{
  las = read_las(f, expose = "*")

  expect_equal(dim(las), c(62, 19))
})

test_that("callback coverage",
{
  # shuffle the data
  fun = function(data) { data[sample(1:nrow(data)), ] }
  read = reader_las()
  call = callback(fun, expose = "*")

  expect_error(exec(read+call, on = f), NA)
})

test_that("callback fails nicely",
{
  read = reader_las()
  call = callback(test_stop, expose = "xyzi")
  write = write_las()
  pipeline = read + call + write
  expect_error(exec(pipeline, on = f), "error in test_stop")

  call = callback(unname, expose = "xyzi")
  pipeline = read + call
  expect_error(exec(pipeline, on = f), "the data.frame has no names")
})

test_that("callback works with many args",
{
  fun = function(data,a,b,c,d,e,f) { return(a+b+c+d+e+f) }
  read = reader_las()
  call = callback(fun = fun, expose = "xyzi", a = 0, b = 1, c = 2, d = 3, e = 4, f = 5)
  pipeline = read + call
  ans = exec(pipeline, on = f)
  expect_equal(ans, 15)
})




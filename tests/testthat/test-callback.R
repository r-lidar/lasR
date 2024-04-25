read_las = function(f, expose = "xyzi")
{
  load = function(data) { return(data) }
  read = reader(f)
  call = callback(load, expose, no_las_update = TRUE)
  return (processor(read+call))
}

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
  las = read_las(f)

  expect_s3_class(las, "data.frame")
  expect_equal(dim(las), c(30L,4L))
  expect_equal(attr(las, "bbox"), c(339002.9, 5248000.0,  339015.1, 5248001.2), tolerance = 0.01)
})

test_that("callback edits and deletes the points",
{
  read = reader(f)
  call = callback(convert_intensity_in_range, expose = "xyzi", min = 0, max = 255)
  write = write_las()
  pipeline = read + call + write
  ans = processor(pipeline)

  las = read_las(ans)

  expect_s3_class(las, "data.frame")
  expect_equal(range(las$Intensity), c(11,223))
  expect_equal(nrow(las), 15L, tolerance = 1)
})

test_that("callback cannot add an attribute not compatble with the format.",
{
  fun = function(data) { data$R = 0L ; data }
  read = reader(f)
  call = callback(fun, expose = "xyzi")
  pipeline = read + call
  expect_error(processor(pipeline), "non supported column 'R'")
})

test_that("callback fails if an attribute is of incorrect type.",
{
  fun = function(data) { data$Classification = 0 ; data }
  read = reader(f)
  call = callback(fun, expose = "xyzi")
  pipeline = read + call
  expect_error(processor(pipeline), "Classification is expected to be integer")
})

test_that("callback without ouput and multiple file",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts/")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  read = reader(f, filter = "-keep_every_nth 100", buffer = 50)
  call = callback(function(data) { data }, expose = "xyzi", no_las_update = TRUE)
  pipeline = read + call
  ans = processor(pipeline)

  expect_type(ans, "list")
  expect_length(ans, 2)
  expect_equal(attr(ans[[1]], "bbox"), c(885022.4, 629157.2, 885210.2, 629400.0), tolerance = 0.01)
  expect_equal(attr(ans[[2]], "bbox"), c(885024.1, 629400.0, 885217.1, 629700.0), tolerance = 0.01)
  expect_equal(range(ans[[1]]$Y)[2], 629400.0 + 50, tolerance = 1e-7)
  expect_equal(range(ans[[2]]$Y)[1], 629400.0 - 50, tolerance = 1e-7)
  expect_equal(nrow(ans[[1]]), 6820)
  expect_equal(nrow(ans[[2]]), 9315)
})

test_that("callback can return any object",
{
  meanz = function(data){ return(mean(data$Z)) }
  read = reader(f)
  call = callback(meanz, expose = "xyz")
  ans = processor(read+call)

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
  read = reader(f, xc = x, yc = y, r = r, buffer = 10)
  pipeline = read + call1 + call2

  ans = processor(pipeline)
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
  read = reader(f)
  call = callback(fun, expose = "*")

  expect_error(processor(read+call), NA)
})

test_that("callback fails nicely",
{
  read = reader(f)
  call = callback(test_stop, expose = "xyzi")
  write = write_las()
  pipeline = read + call + write
  expect_error(processor(pipeline), "error in test_stop")

  call = callback(unname, expose = "xyzi")
  pipeline = read + call
  expect_error(processor(pipeline), "the data.frame has no names")
})

test_that("callback works with many args",
{
  fun = function(data,a,b,c,d,e,f) { return(a+b+c+d+e+f) }
  read = reader(f)
  call = callback(fun = fun, expose = "xyzi", a = 0, b = 1, c = 2, d = 3, e = 4, f = 5)
  pipeline = read + call
  ans = processor(pipeline)
  expect_equal(ans, 15)
})




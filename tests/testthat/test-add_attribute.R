f = system.file("extdata", "Example.las", package="lasR")

fun = function(data) { data$HAG = 0; return(data) }
gun = function(data) { data }

test_that("callback cannot add attribute without EB",
{
  expect_error(exec(callback(fun), on = f), "non supported column 'HAG'")
})

test_that("callback can add attribute with EB",
{
  pipeline = add_extrabytes("double", "HAG", "test") + callback(fun)
  expect_warning(exec(pipeline, f), NA)
})

test_that("add_extrabytes work in chain",
{
  pipeline = add_extrabytes("double", "HAG", "test")  +
    add_extrabytes("int", "VAL", "test2") +
    callback(gun, expose = "xyz01", no_las_update = T)

  ans = exec(pipeline, f)

  expect_true("HAG" %in% names(ans))
  expect_true("VAL" %in% names(ans))
})

test_that("add_extrabytes produced writable and readable file (#2)",
{
  f1 = system.file("extdata", "Example.las", package="lasR")
  f2 = system.file("extdata", "Example.laz", package="lasR")
  g1 = tempfile(fileext = ".las")
  g2 = tempfile(fileext = ".laz")
  w1 = write_las(g1)
  w2 = write_las(g2)

  extra = add_extrabytes("double", "HAG", "test")

  ans1 = exec(extra+w1, f1, noread = T)
  ans2 = exec(extra+w2, f2, noread = T)

  expect_error(u1 <- exec(summarise(), ans1), NA)
  expect_error(u2 <- exec(summarise(), ans2), NA)

  expect_equal(u1$npoints, 30L)
  expect_equal(u2$npoints, 30L)
  expect_equal(u1$nsingle, 24L)
  expect_equal(u2$nsingle, 24L)
})

test_that("callback and add_attribute work with EB of any type",
{
  g = tempfile(fileext = ".las")

  fun = function(data)
  {
    data$attr1 = as.integer(seq(-2^4, 2^8, length.out = 30))
    data$attr2 = as.integer(seq(-2^4, 2^8, length.out = 30))
    data$attr3 = as.integer(seq(-2^4, 2^16, length.out = 30))
    data$attr4 = as.integer(seq(-2^4, 2^16, length.out = 30))
    #data$attr5 = as.integer(seq(-2^4, 2^16, length.out = 30))
    data$attr6 = as.integer(seq(-2^4, 2^16, length.out = 30))
    data$attr7 = seq(-2^4, 2^16, length.out = 30)
    data
  }

  pipeline = reader_las() +
    add_extrabytes("uchar", "attr1", "test") +
    add_extrabytes("char", "attr2", "test") +
    add_extrabytes("ushort", "attr3", "test") +
    add_extrabytes("short", "attr4", "test") +
    #add_extrabytes("uint", "attr5", "test") +
    add_extrabytes("int", "attr6", "test") +
    add_extrabytes("float", "attr7", "test") +
    callback(fun, expose = "01234567") +
    write_las(g)

  expect_error(exec(pipeline, f), NA)

  read_las = function(f, expose = "xyzi")
  {
    load = function(data) { return(data) }
    call = callback(load, expose, no_las_update = TRUE)
    return (exec(call, f))
  }

  data = read_las(g, expose = "*")

  expect_true(all(c("attr1", "attr2", "attr3", "attr4", "attr6", "attr7") %in% names(data)))
  expect_equal(range(data$attr1), c(0,255))
  expect_equal(range(data$attr2), c(-16,127))
  expect_equal(range(data$attr3), c(0,65535))
  expect_equal(range(data$attr4), c(-16,32767))
  expect_equal(range(data$attr6), c(-16,65536))
  expect_equal(range(data$attr7), c(-16,65536))
  expect_true(is.integer(data$attr1))
  expect_true(is.integer(data$attr2))
  expect_true(is.integer(data$attr3))
  expect_true(is.integer(data$attr4))
  expect_true(is.integer(data$attr6))
  expect_true(is.double(data$attr7))
})

test_that("Cannot add an attribute with reserved keyword",
{
  pipeline <- lasR::reader() + lasR::add_extrabytes("float", "angle", "Random numbers")
  expect_error(lasR::exec(pipeline, on = f), "is a reserved word interpreted as 'ScanAngle'")
})


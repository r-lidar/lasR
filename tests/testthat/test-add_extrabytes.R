f = system.file("extdata", "Example.las", package="lasR")

fun = function(data) { data$HAG = 0; return(data) }
gun = function(data) { data }

test_that("callback cannot add attribute without EB",
{
  pipeline = reader(f) + callback(fun)
  expect_warning(processor(pipeline), "non supported column 'HAG'")
})

test_that("callback can add attribute witht EB",
{
  pipeline = reader(f) + add_extrabytes("double", "HAG", "test") + callback(fun)
  expect_warning(processor(pipeline), NA)
})

test_that("add_extrabytes work in chain",
{
  pipeline = reader(f) +
    add_extrabytes("double", "HAG", "test")  +
    add_extrabytes("int", "VAL", "test2") +
    callback(gun, expose = "xyz01", no_las_update = T)

  ans = processor(pipeline)

  expect_true("HAG" %in% names(ans))
  expect_true("VAL" %in% names(ans))
})

test_that("add_extrabytes produced writable and readable file (#2)",
{
  f1 = system.file("extdata", "Example.las", package="lasR")
  f2 = system.file("extdata", "Example.laz", package="lasR")
  g1 = tempfile(fileext = ".las")
  g2 = tempfile(fileext = ".laz")
  r1 = reader(f1)
  r2 = reader(f2)
  w1 = write_las(g1)
  w2 = write_las(g2)

  extra = add_extrabytes("double", "HAG", "test")

  ans1 = processor(r1+extra+w1, noread = T)
  ans2 = processor(r2+extra+w2, noread = T)

  expect_error(u1 <- processor(reader(ans1) + summarise()), NA)
  expect_error(u2 <- processor(reader(ans2) + summarise()), NA)

  expect_equal(u1$npoints, 30L)
  expect_equal(u2$npoints, 30L)
  expect_equal(u1$nsingle, 24L)
  expect_equal(u2$nsingle, 24L)
})

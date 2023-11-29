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

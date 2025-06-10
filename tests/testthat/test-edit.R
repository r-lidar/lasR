test_that("Edit attribute works", {
  f <- system.file("extdata", "Example.las", package="lasR")

  edit = edit_attribute(filter = c("Z < 975", "Z > 974"), attribute = "UserData", value = 2)
  io = write_las(templas())
  pipeline = edit + io
  ans = exec(pipeline, on = f)

  res = read_las(ans, expose = "xyzu")

  cond = res$Z < 975 & res$Z > 974
  expect_true(all(res$UserData[cond] == 2))
  expect_true(all(res$UserData[!cond] == 32))
})

test_that("Edit attribute fails", {
  f <- system.file("extdata", "Example.las", package="lasR")

  edit = edit_attribute(filter = "Z < 975", attribute = "ABC", value = 2)
  expect_error(exec(edit, on = f), "No attribute 'ABC' found")

  expect_error(edit_attribute(filter = "Z < 975", attribute = "Z", value = 2), "Editing point coordinates is not allowed")
})


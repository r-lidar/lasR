test_that("li2012 errors when the store_in_attribute is missing", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  # segID is not present in MixedConifer.las and we do not add it.
  pipeline <- reader_las() + li2012(store_in_attribute = "segID")
  expect_error(
    exec(pipeline, on = f),
    regexp = "segID is not present in the point cloud"
  )
})

test_that("li2012 errors when store_in_attribute has wrong type", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  # Declare segID as float instead of int -> should be rejected.
  addid <- add_extrabytes("float", "segID", "wrong type")
  pipeline <- reader_las() + addid + li2012(store_in_attribute = "segID")
  expect_error(
    exec(pipeline, on = f),
    regexp = "segID must be of type 'int' with scale=1 and offset=0"
  )
})

test_that("li2012 errors when store_in_attribute has wrong scale", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "wrong scale", scale = 0.01)
  pipeline <- reader_las() + addid + li2012(store_in_attribute = "segID")
  expect_error(
    exec(pipeline, on = f),
    regexp = "segID must be of type 'int' with scale=1 and offset=0"
  )
})

# Read per-point data back into R via lasR's own callback idiom.
# expose="*" surfaces all attributes (incl. custom extrabytes) by name.
read_points <- function(path, expose = "*") {
  identity_cb <- function(data) data
  exec(callback(identity_cb, expose = expose, no_las_update = TRUE),
       on = path)
}

test_that("li2012 yields all-NA when hmin exceeds the highest point", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  seg <- li2012(hmin = 1e6, store_in_attribute = "segID")  # above tallest point
  out_path <- tempfile(fileext = ".laz")
  wlas <- write_las(ofile = out_path)
  # The stage warns via REprintf (stderr), not an R condition, so we assert
  # the observable contract: with hmin above the highest point, no tree is
  # segmented and every point is NA.
  exec(reader_las() + addid + seg + wlas, on = f)
  d <- read_points(out_path)
  expect_true(all(is.na(d$segID)))
})

test_that("li2012 produces non-NA tree IDs on MixedConifer", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  seg <- li2012(store_in_attribute = "segID")
  out_path <- tempfile(fileext = ".laz")
  wlas <- write_las(ofile = out_path)
  exec(reader_las() + addid + seg + wlas, on = f)
  d <- read_points(out_path)
  expect_gt(sum(!is.na(d$segID)), 0)
  ids <- na.omit(d$segID)
  expect_true(all(ids > 0))          # tree IDs are positive (register_apex pre-increments from 0)
  expect_true(all(ids == as.integer(ids)))
})

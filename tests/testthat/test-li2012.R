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

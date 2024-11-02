test_that("buffer tiles",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  read = reader_las()
  write = write_las(paste0(tempdir(), "/*_buffered.las"), keep_buffer = TRUE)
  ans = exec(read+write, on = f, buffer = 25)

  cont1 = exec(reader_las() + hulls(), on = f)
  cont2 = exec(reader_las() + hulls(), on = ans)

  expect_equal(dim(cont1), c(4L,1L))

  area = function(m) { diff(m[1:2,1]) * diff(m[2:3,2])}
  area1 = as.numeric(sum(sf::st_area(cont1)))
  area2 = as.numeric(sum(sf::st_area(cont2)))

  expect_equal(area1, 206788.110)
  expect_equal(area2, 236796.375)
})

test_that("normalize & dtm",
{
  f <- system.file("extdata", "Topography.las", package="lasR")

  pipeline = reader_las() + dtm() + normalize() + summarise() + chm(2)
  suppressWarnings(ans <- exec(pipeline, on = f))

  expect_length(ans, 3L)
  expect_s4_class(ans[[1]], "SpatRaster")

  x = as.numeric(names(ans$summary$z_histogram))
  w = ans$summary$z_histogram

  expect_equal(mean(x*w/sum(w)), 0.31, tolerance = 0.01)

  pipeline = reader_las() + dtm(add_class = 9) + hag() + summarise() + chm(1, TRUE)
  suppressWarnings(ans <- exec(pipeline, on = f))

  expect_length(ans, 3L)
  expect_s4_class(ans[[1]], "SpatRaster")

  x = as.numeric(names(ans$summary$z_histogram))
  w = ans$summary$z_histogram

  # was not actually normalized because put in extrabyte
  expect_equal(mean(x*w/sum(w)), 36.77, tolerance = 0.01)
})

test_that("pipleline info works",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  pipeline = triangulate() + rasterize(1)
  info = lasR:::get_pipeline_info(pipeline)

  expect_equal(info$streamable, FALSE)
  expect_equal(info$buffer, 20)
  expect_equal(info$read_points, TRUE)

  pipeline = hulls()
  info = lasR:::get_pipeline_info(pipeline)

  expect_equal(info$streamable, TRUE)
  expect_equal(info$buffer, 0)
  expect_equal(info$read_points, FALSE)

  pipeline = rasterize(10)
  info = lasR:::get_pipeline_info(pipeline)

  expect_equal(info$streamable, TRUE)
  expect_equal(info$buffer, 0)
  expect_equal(info$read_points, TRUE)

  pipeline[[1]]$algoname = "plop"
  expect_error(lasR:::get_pipeline_info(pipeline), "Unsupported stage: plop")
})

test_that("processor does not fails without reader",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  #expect_error(exec(hulls(), on = f), "The pipeline must have a readers stage")
  expect_error(exec(local_maximum(10) + reader_las(f), on = f),  "not preceded by a reader stage")
  expect_error(exec(hulls() + reader_las(f), on = f),  "A 'reader' stage is missing or is at an incorrect position in the pipeline")
})

test_that("delete point memory reallocation works",
{
  f <- system.file("extdata", "MixedConifer.las", package="lasR")
  pipeline = delete_points(filter = keep_ground()) + geometry_features(k = 15 , features = "lps") + write_las()
  expect_error(exec(pipeline, on = f), NA)
})


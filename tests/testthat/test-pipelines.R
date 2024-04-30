test_that("buffer tiles",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  read = reader_las()
  write = write_las(paste0(tempdir(), "/*_buffered.las"), keep_buffer = TRUE)
  ans = exec(read+write, on = f, buffer = 25)

  cont1 = exec(reader_las() + hulls(), on = f)
  cont2 = exec(reader_las() + hulls(), on = ans)

  expect_equal(dim(cont1), c(4L,1L))

  # Not using sf::st_area that fails on github action because of missing crs data.base
  area = function(m) { diff(m[1:2,1]) * diff(m[2:3,2])}
  area1 = 0; for (i in 1:4) area1 = area1 + area(cont1$geom[[i]][[1]])
  area2 = 0; for (i in 1:4) area2 = area2 + area(cont2$geom[[i]][[1]])

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

  expect_equal(mean(x*w/sum(w)), 0.287, tolerance = 0.01)

  pipeline = reader_las() + dtm(add_class = 9) + normalize(TRUE) + summarise() + chm(1, TRUE)
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
  expect_equal(info$buffer, 50)
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

test_that("processor fails without reader",
{
  f <- system.file("extdata", "bcts/", package="lasR")

  expect_error(exec(hulls() + reader_las(), on = f),  "The reader must alway be the first stage of the pipeline")
})

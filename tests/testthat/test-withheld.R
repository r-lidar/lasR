test_that("stages are skipping withhelded points (streaming)", {

  f <- system.file("extdata", "Megaplot.las", package="lasR")
  olas <- templas()
  read <- reader_las()
  filter <- delete_points("Z < 10")
  pipeline <- read + filter + summarise() + rasterize(10, "min") + write_las(olas)

  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 56204L)
  expect_equal(mean(ans$rasterize[], na.rm = T), 10.38996, tolerance = 1e-6)
  expect_equal(sum(is.na(ans$rasterize[])), 96)

  pipeline = summarise()
  ans = exec(pipeline, on = olas)
  expect_equal(ans$npoints, 56204L)
})

test_that("stages are skipping withhelded points (batch)", {

  f <- system.file("extdata", "Megaplot.las", package="lasR")
  olas <- templas()
  otri <- tempgpkg()
  read <- reader_las()
  filter <- delete_points("Z < 20")
  pipeline <- read + filter + triangulate(-10, ofile = otri) + summarise() + rasterize(10, "min") + write_las(olas)

  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 16791L)
  expect_equal(mean(ans$rasterize[], na.rm = T), 20.10289, tolerance = 1e-6)
  expect_equal(sum(is.na(ans$rasterize[])), 192)
  expect_equal(length(ans$triangulate$geom[[1]]), 468L)

  pipeline = summarise()
  ans = exec(pipeline, on = olas)
  expect_equal(ans$npoints, 16791L)
})

test_that("stages are skipping withhelded points (2) (batch)", {

  f <- system.file("extdata", "Megaplot.las", package="lasR")
  olas <- templas()
  otri <- tempgpkg()
  read <- reader_las()
  filter <- delete_points("Z >= 10")
  dtm = dtm()
  pipeline <- read + filter + local_maximum(5) + dtm + transform_with(dtm[[2]]) + lasR::summarise()

  set_parallel_strategy(sequential())
  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 25379L)
  expect_equal(nrow(ans$local_maximum), 1516L)
})


test_that("voxel sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_voxel(5)
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  skip_on_os("mac") # because shuffle has different implementation and lead to slightly different valid outcomes

  expect_equal(ans$summary$npoints, 7864L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 5912, `2` = 1496, `3` = 394, `4` = 57, `5` = 4, `6` = 1))
})

test_that("voxel sampling respect filter",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_voxel(5, filter = keep_first())
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  skip_on_os("mac") # because shuffle has different implementation and lead to slightly different valid outcomes

  expect_equal(ans$summary$npoints, 27533L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 7668, `2` = 15828, `3` = 3569, `4` = 451, `5` = 16, `6` = 1))
})


test_that("pixel sampling works",
 {
   f = system.file("extdata", "Topography.las", package="lasR")
   o = tempfile(fileext = ".las")
   vox = sampling_pixel(5)
   write = write_las(o)
   pipeline = vox + write + summarise()
   ans = exec(pipeline, on = f)

   skip_on_os("mac") # because shuffle has different implementation and lead to slightly different valid outcomes

   expect_equal(ans$summary$npoints, 3042L)
   expect_equal(ans$summary$npoints_per_return, c(`1` = 2352, `2` = 547, `3` = 128, `4` = 14, `6` = 1))
})

test_that("pixel sampling respect filter",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_pixel(5, filter = keep_first())
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  skip_on_os("mac") # because shuffle has different implementation and lead to slightly different valid outcomes

  expect_equal(ans$summary$npoints, 22907)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 3042, `2` = 15828, `3` = 3569, `4` = 451, `5` = 16, `6` = 1))
})

test_that("poisson sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_poisson(5)
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  skip_on_os("mac") # because shuffle has different implementation and lead to slightly different valid outcomes

  expect_equal(ans$summary$npoints, 4669L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 3596, `2` = 785, `3` = 238, `4` = 48, `5` = 1, `6` = 1))
})

test_that("poisson sampling respect filter",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_poisson(5, filter = keep_first())
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  skip_on_os("mac") # because shuffle has different implementation and lead to slightly different valid outcomes

  expect_equal(ans$summary$npoints, 24281L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 4416, `2` = 15828, `3` = 3569, `4` = 451, `5` = 16, `6` = 1))
})


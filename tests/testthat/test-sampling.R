test_that("voxel sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_voxel(5)
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

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

  expect_equal(ans$summary$npoints, 7668L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 7668L))
})


test_that("pixel sampling works",
 {
   f = system.file("extdata", "Topography.las", package="lasR")
   o = tempfile(fileext = ".las")
   vox = sampling_pixel(5)
   write = write_las(o)
   pipeline = vox + write + summarise()
   ans = exec(pipeline, on = f)

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

  expect_equal(ans$summary$npoints, 3042L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 3042L))
})

test_that("poisson sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_poisson(5)
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 4136L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 3153, `2` = 758, `3` = 193, `4` = 30, `5` = 2))
})

test_that("poisson sampling respect filter",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_poisson(5, filter = keep_first())
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 3928L)
  expect_equal(ans$summary$npoints_per_return, c(`1` = 3928L))
})


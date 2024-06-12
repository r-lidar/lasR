test_that("voxel sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_voxel(5)
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 7864L)
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
})

test_that("poisson sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_poisson(5)
  write = write_las(o)
  pipeline = vox + write + summarise()
  ans = exec(pipeline, on = f)

  expect_equal(ans$summary$npoints, 3742L)
})

test_that("voxel sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  vox = sampling_voxel(5)
  write = write_las(o)
  pipeline = vox + write
  exec(pipeline, on = f)

  las = read_las(o, expose = "xyzi")

  expect_equal(dim(las), c(7864L, 4L))
})

test_that("pixel sampling works",
 {
   f = system.file("extdata", "Topography.las", package="lasR")
   o = tempfile(fileext = ".las")
   vox = sampling_pixel(5)
   write = write_las(o)
   pipeline = vox + write
   exec(pipeline, on = f)

   las = read_las(o, expose = "xyzi")

   expect_equal(dim(las), c(3042L, 4L))
})

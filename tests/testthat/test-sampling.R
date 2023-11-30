read_las = function(f, select = "xyzi")
{
  load = function(data) { return(data) }
  read = reader(f)
  call = callback(load, select, no_las_update = TRUE)
  return (processor(read+call))
}

test_that("voxel sampling works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = tempfile(fileext = ".las")
  read = reader(f)
  vox = sampling_voxel(5)
  write = write_las(o)
  pipeline = read + vox + write
  processor(pipeline)

  las = read_las(o)

  expect_equal(dim(las), c(7864L, 4L))
})

test_that("pixel sampling works",
 {
   f = system.file("extdata", "Topography.las", package="lasR")
   o = tempfile(fileext = ".las")
   read = reader(f)
   vox = sampling_pixel(5)
   write = write_las(o)
   pipeline = read + vox + write
   processor(pipeline)

   las = read_las(o)

   expect_equal(dim(las), c(3042L, 4L))
})

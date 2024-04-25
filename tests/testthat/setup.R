if (has_omp_support()) set_parallel_strategy(concurrent_files(2))

read_las = function(f, expose = "*")
{
  load = function(data) { return(data) }
  call = callback(load, expose, no_las_update = TRUE)
  return (exec(call, f))
}

read_crs = function(files)
{
  pipeline = reader_las() + hulls()
  ans = exec(pipeline, on = files)
  return(sf::st_crs(ans))
}

test_that("Has openmp support",
{
  skip_on_os("mac")
  expect_true(has_omp_support())
})
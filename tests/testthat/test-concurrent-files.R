f = paste0(system.file(package="lasR"), "/extdata/bcts/")
f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

test_that("Running pipelines in parallel works with R-based stages", {

  set_parallel_strategy(concurrent_files(2))
  # this is not supposed to crash
  expect_error(ans <- exec(rasterize(10, mean(Z)), on = f), NA)

  expect_equal(dim(ans), c(107,21,1))
  expect_equal(sum(is.na(ans[])), 100L)
  expect_equal(mean(ans[], na.rm = TRUE), 347.5629, tolerance = 1e-6)
})

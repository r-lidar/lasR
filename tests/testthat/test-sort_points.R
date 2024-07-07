test_that("sort works", {
  f = "~/Téléchargements/PRF_LiDAR2012_LAZ_ht/300_5094_PRF_ALLHITS_CGVD28_nor.laz"

  f <- system.file("extdata", "Topography.las", package="lasR")
  expect_error(exec(sort_points(), on = f), NA)
})

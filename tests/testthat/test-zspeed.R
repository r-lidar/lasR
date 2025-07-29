test_that("Computation speed is ok",
{
  file <- "/home/jr/Documents/Ulaval/ALS data/BCTS/092L072244_BCTS_2.laz"
  b = file.exists(file)
  skip_if_not(b, "Skipping local tests")

  b = runif(1) > 0.75
  skip_if_not(b, "Skipping randomly long test (ran only on 25%)")

  las = read_cloud(file)

  dt = system.time(
    ans <- lasR::exec(lasR::geometry_features(8, features = "E"), on = las, ncores = 6, noread = T)
  )

  expect_lt(dt[3], 23)

  # The kdtree has already been built so the expected time is 19 down to 10
  dt = system.time(
    lasR::exec(lasR::classify_with_sor(8,6), on = las, ncores = 6)
  )

  expect_lt(dt[3], 10)

  las = read_cloud(file)
  dt = system.time(
    lasR::exec(lasR::normalize(), on = las, ncores = 6, progress = T)
  )

  expect_lt(dt[3], 6)
})

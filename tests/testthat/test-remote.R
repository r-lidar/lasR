test_that("remote COPC reads correctly via HTTP",
{
  skip_if_not_installed("httpuv")

  f <- system.file("extdata", "example.copc.laz", package = "lasR")
  data_dir <- dirname(f)

  # Start a local HTTP server
  port   <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/example.copc.laz")

  # Read remote
  pipeline_remote <- reader() + summarise()
  ans_remote <- exec(pipeline_remote, on = url)

  # Read local
  pipeline_local <- reader() + summarise()
  ans_local <- exec(pipeline_local, on = f)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote COPC with copc_depth works",
{
  skip_if_not_installed("httpuv")

  f <- system.file("extdata", "example.copc.laz", package = "lasR")
  data_dir <- dirname(f)

  port   <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/example.copc.laz")

  pipeline_remote <- reader(depth = 0) + summarise()
  ans_remote <- exec(pipeline_remote, on = url)

  pipeline_local <- reader(depth = 0) + summarise()
  ans_local <- exec(pipeline_local, on = f)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote non-COPC file reads correctly",
{
  skip_if_not_installed("httpuv")

  f <- system.file("extdata", "Example.laz", package = "lasR")
  data_dir <- dirname(f)

  port <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/Example.laz")

  pipeline_remote <- reader() + summarise()
  ans_remote <- exec(pipeline_remote, on = url)

  pipeline_local <- reader() + summarise()
  ans_local <- exec(pipeline_local, on = f)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote file with invalid URL fails gracefully",
{
  expect_error(exec(reader() + summarise(), on = "https://localhost:65535/nonexistent.copc.laz"))
})

test_that("public remote COPC endpoint works",
{
  skip_if_not(nzchar(Sys.which("curl")) || capabilities("libcurl"), "No network available")

  url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"

  pipeline <- reader(depth = 0) + summarise()
  ans <- exec(pipeline, on = url)

  expect_true(ans$npoints > 0)
  expect_true(ans$npoints < 100000)

  pipeline <- reader(depth = 1) + summarise()
  ans <- exec(pipeline, on = url)

  expect_true(ans$npoints > 100000)
  expect_true(ans$npoints < 140000)
})

test_that("Spatial query from remote copc file",
{
  url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"

  pipeline <-  reader_circles(637368.8, 851944.8, 15) + summarise()
  ans <- exec(pipeline, on = url)

  expect_equal(ans$npoints, 831)
})


test_that("Build a VPC file from remote file",
{
  url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"

  o = tempfile(fileext = ".vpc")
  pipeline <- write_vpc(o)
  ans <- exec(pipeline, on = url)

  ans = sf::st_read(ans, quiet = TRUE, stringsAsFactors = TRUE)

  expect_true(file.exists(o))
  expect_equal(ans$pc.count, 10653336)
  expect_equal(ans$`proj:bbox`[[1]], c(635577.79, 848882.150, 639003.730, 853537.660))
})

test_that("remote EPT reads correctly via HTTP",
{
  skip_if_not_installed("httpuv")

  ept_local <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  data_dir <- dirname(ept_local)

  port <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/ept.json")

  ans_remote <- exec(reader() + summarise(), on = url)
  ans_local <- exec(reader() + summarise(), on = ept_local)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote EPT with depth works",
{
  skip_if_not_installed("httpuv")

  ept_local <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  data_dir <- dirname(ept_local)

  port <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/ept.json")

  ans_full <- exec(reader() + summarise(), on = url)
  ans_d1 <- exec(reader(depth = 1) + summarise(), on = url)

  expect_equal(ans_d1$npoints, ans_full$npoints)
})

test_that("remote EPT under concurrent_files matches sequential",
{
  skip_if_not_installed("httpuv")
  skip_if_not(has_omp_support())

  # Note: a counting custom app$call handler cannot be used here because
  # httpuv routes app$call through the R event loop, which is blocked by
  # the synchronous lasR exec call (deadlock). Existing remote tests use
  # the C-native staticPaths path for this reason. The metadata-cache
  # invariant (one ept.json fetch per pipeline, not per-chunk) is asserted
  # at the C++ layer via cpp_ept_partition_inspect's tiles_built flag in
  # tests/testthat/test-ept.R; this test verifies the parallel remote path
  # produces identical results to sequential, exercising the same shared
  # HierarchyIndex code paths over HTTP.

  ept_local <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  data_dir <- dirname(ept_local)

  port   <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port,
                                list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/ept.json")

  ans_seq <- exec(reader() + summarise(), on = url, ncores = sequential())
  ans_par <- exec(reader() + summarise(), on = url,
                  ncores = concurrent_files(4))

  expect_gt(ans_seq$npoints, 0)
  expect_equal(ans_par$npoints, ans_seq$npoints)
  expect_equal(ans_par$z_histogram, ans_seq$z_histogram)
})

test_that("remote EPT scopes its GDAL tuning to the exec and restores the options",
{
  skip_if_not_installed("httpuv")
  old_env <- unset_ept_gdal_env()
  on.exit(restore_env(old_env), add = TRUE)

  opts <- function(path = "") lasR:::.APITEST$cpp_gdal_options(ept_gdal_keys, path)
  expect_true(all(is.na(opts())))

  ept_local <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  port   <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = dirname(ept_local))))
  on.exit(httpuv::stopServer(server), add = TRUE)

  base  <- paste0("http://127.0.0.1:", port, "/")
  url   <- paste0(base, "ept.json")
  tile  <- paste0("/vsicurl/", base, "ept-data/1-0-0-0.laz")
  other <- "/vsicurl/http://example.invalid/data/raster.tif"

  # While the endpoint is open the tuning applies to its tiles
  during <- lasR:::.APITEST$cpp_ept_gdal_options_while_open(url, ept_gdal_keys, c(tile, other))
  expect_equal(unname(during[, 1]), c("EMPTY_DIR", "YES", "TRUE", "67108864"))

  # Where GDAL can scope an option to a path, directory listing is disabled only under the endpoint
  if (lasR:::.APITEST$cpp_gdal_path_specific_options())
    expect_true(is.na(during[["GDAL_DISABLE_READDIR_ON_OPEN", 2]]))

  # Nothing is left behind once the endpoint is closed
  expect_true(all(is.na(opts())))
  expect_true(all(is.na(opts(tile))))

  old <- get_parallel_strategy()
  on.exit(if (is.null(old)) unset_parallel_strategy() else set_parallel_strategy(old), add = TRUE)

  strategies <- list(sequential())
  if (has_omp_support()) strategies <- c(strategies, list(concurrent_files(2)))

  for (strategy in strategies)
  {
    set_parallel_strategy(strategy)
    ans <- exec(reader() + summarise(), on = url)
    expect_gt(ans$npoints, 0)
    expect_true(all(is.na(opts())))
    expect_true(all(is.na(opts(tile))))
  }
})

test_that("a GDAL option set by the user wins over the remote EPT tuning and stays in control",
{
  skip_if_not_installed("httpuv")
  old_env <- unset_ept_gdal_env()
  on.exit(restore_env(old_env), add = TRUE)

  ept_local <- system.file("extdata", "ept-test-multi", "ept.json", package = "lasR")
  port   <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = dirname(ept_local))))
  on.exit(httpuv::stopServer(server), add = TRUE)

  base <- paste0("http://127.0.0.1:", port, "/")
  url  <- paste0(base, "ept.json")
  tile <- paste0("/vsicurl/", base, "ept-data/1-0-0-0.laz")
  opts <- function(path = "") lasR:::.APITEST$cpp_gdal_options(ept_gdal_keys, path)

  Sys.setenv(VSI_CACHE = "FALSE")
  during <- lasR:::.APITEST$cpp_ept_gdal_options_while_open(url, ept_gdal_keys, tile)
  expect_equal(during[["VSI_CACHE", 1]], "FALSE")
  expect_equal(during[["GDAL_DISABLE_READDIR_ON_OPEN", 1]], "EMPTY_DIR")

  exec(reader() + summarise(), on = url)
  expect_equal(opts()[["VSI_CACHE"]], "FALSE")

  # Environment variables set after an EPT read still take effect
  Sys.unsetenv("VSI_CACHE")
  Sys.setenv(GDAL_DISABLE_READDIR_ON_OPEN = "NO")
  expect_true(is.na(opts()[["VSI_CACHE"]]))
  expect_equal(opts(tile)[["GDAL_DISABLE_READDIR_ON_OPEN"]], "NO")
})

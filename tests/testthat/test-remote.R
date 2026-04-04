test_that("remote COPC reads correctly via HTTP",
{
  skip_on_cran()
  skip_on_os("windows")
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
  skip_on_cran()
  skip_on_os("windows")
  skip_if_not_installed("httpuv")

  f <- system.file("extdata", "example.copc.laz", package = "lasR")
  data_dir <- dirname(f)

  port   <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port, list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/example.copc.laz")

  pipeline_remote <- reader(copc_depth = 0) + summarise()
  ans_remote <- exec(pipeline_remote, on = url)

  pipeline_local <- reader(copc_depth = 0) + summarise()
  ans_local <- exec(pipeline_local, on = f)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote non-COPC file reads correctly",
{
  skip_on_cran()
  skip_on_os("windows")
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
  skip_on_cran()
  skip_on_os("windows")

  expect_error(exec(reader() + summarise(), on = "https://localhost:65535/nonexistent.copc.laz"))
})

test_that("public remote COPC endpoint works",
{
  skip_on_cran()
  skip_on_os("windows")
  skip_if_not(nzchar(Sys.which("curl")) || capabilities("libcurl"), "No network available")

  url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"

  pipeline <- reader(copc_depth = 0) + summarise()
  ans <- exec(pipeline, on = url)

  expect_true(ans$npoints > 0)
  expect_true(ans$npoints < 100000)

  pipeline <- reader(copc_depth = 1) + summarise()
  ans <- exec(pipeline, on = url)

  expect_true(ans$npoints > 100000)
  expect_true(ans$npoints < 140000)
})

test_that("Spatial query from remote copc file",
{
  skip_on_cran()
  skip_on_os("windows")
  skip_if_not(nzchar(Sys.which("curl")) || capabilities("libcurl"), "No network available")

  url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"

  pipeline <-  reader_circles(637368.8, 851944.8, 15) + summarise()
  ans <- exec(pipeline, on = url)

  expect_equal(ans$npoints, 831)
})


test_that("Build a VPC file from remote file",
{
  skip_on_cran()
  skip_on_os("windows")
  skip_if_not(nzchar(Sys.which("curl")) || capabilities("libcurl"), "No network available")

  url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"

  o = tempfile(fileext = ".vpc")
  pipeline <- write_vpc(o)
  ans <- exec(pipeline, on = url)

  ans = sf::st_read(ans, quiet = TRUE, stringsAsFactors = TRUE)

  expect_true(file.exists(o))
  expect_equal(ans$pc.count, 10653336)
  expect_equal(ans$`proj:bbox`[[1]], c(635577.79, 848882.150, 639003.730, 853537.660))
})

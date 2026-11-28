test_that("writelas works",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  o = paste0(tempdir(), "/test.las")
  reader = reader_las(filter = "-keep_first")
  writer = write_las(o)
  u = exec(reader + writer, on = f)

  expect_type(u, "character")
  expect_true(all(file.exists(u)))
  expect_length(u, 1)

  v = exec(summarise(), on = o)

  expect_equal(v$npoints, 53538)
  expect_equal(v$npoints_per_return, c("1" = 53538L))
  expect_equal(v$npoints_per_class, c("1" = 44151L, "2" = 5490L, "9" = 3897L))
})

test_that("writelas writes 4 files",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

  reader = reader_las(filter = "-keep_first")
  writer = write_las()
  u = exec(reader + writer, on = f)

  f1 = sub(pattern = "(.*)\\..*$", replacement = "\\1", basename(f))
  f1 = paste0(f1, ".las")

  expect_type(u, "character")
  expect_length(u, 4)
  expect_equal(basename(u), f1)
  expect_true(all(file.exists(u)))
})

test_that("writelas writes 4 files with a buffered pipeline",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  f = f[1:2]

  reader = reader_las(filter = "-keep_every_nth 10 -keep_first")
  writer = write_las(keep_buffer = F)
  u = exec(reader + writer, on = f, buffer = 50)
  h = exec(hulls(), on = u)

  expect_equal(as.numeric(sum(sf::st_area(h))), 103361.154)

  reader = reader_las(filter = "-keep_every_nth 10 -keep_first")
  writer = write_las(keep_buffer = T)
  u = exec(reader + writer, on = f, buffer = 50)
  h = exec(hulls(), on = u)

  expect_equal(as.numeric(sum(sf::st_area(h))), 122816.069)
})

test_that("writelas writes 1 merged file",
{
  f = paste0(system.file(package="lasR"), "/extdata/bcts")
  f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)
  o = paste0(tempdir(), "/test.las")

  reader = reader_las(filter = "-keep_first")
  writer = write_las(o)
  u = exec(reader + writer, on = f)

  expect_type(u, "character")
  expect_length(u, 1)
  expect_true(all(file.exists(u)))

  v = exec(reader_las() + summarise(), on = o)

  expect_equal(v$npoints, 1981696L)
  expect_equal(v$npoints_per_return, c("1" = 1981696L))
  expect_equal(v$npoints_per_class, c("1" = 1899830L, "2" = 81866L))
})

test_that("Cannot overwrite processed file",
{
  file <- system.file("extdata", "MixedConifer.las", package = "lasR")
  pipeline = set_crs(25832) + write_las(ofile = file)
  expect_error(exec(pipeline, on = file), "Cannot override a file used as a source of point-cloud")
})

test_that("writelas allow controlling format",
{
  f = paste0(system.file(package="lasR"), "/extdata/Example.las")
  o = paste0(tempdir(), "/test.las")

  reader = reader_las(filter = "-keep_first")
  writer = write_las(o, version = 1, pdrf = 0)
  u = exec(reader + writer, on = f)

  expect_type(u, "character")
  expect_length(u, 1)
  expect_true(all(file.exists(u)))

  v = exec(reader_las() + summarise(), on = o)

  expect_equal(v$npoints, 26)
  expect_equal(v$npoints_per_return, c("1" = 26))
  expect_equal(v$npoints_per_class, c("1" = 25, "2" = 1))
})


test_that("writelas replaces tags in the file name",
{
  f = system.file("extdata", "Topography.las", package="lasR")
  d = tempfile()
  dir.create(d)

  read = reader_rectangles(c(273400, 273500), c(5274400,5274400), c(273450,273550), c(5274450,5274460))
  write = write_las(file.path(d, "{XCENTER}_{YCENTER}_{XMIN}_{XMAX}_{YMIN}_{YMAX}.las"))
  u = exec(read + write, on = f)

  expect_equal(basename(u), c("273425_5274425_273400_273450_5274400_5274450.las", "273525_5274430_273500_273550_5274400_5274460.las"))
  expect_true(all(file.exists(u)))

  u = exec(write_las(file.path(d, "{CHUNKNAME}_tile.las")), on = f, chunk = 200)
  expect_length(u, 9)
  expect_equal(basename(u), paste0("Topography_", 0:8, "_tile.las"))
})

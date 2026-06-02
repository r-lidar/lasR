test_that("li2012 errors when the store_in_attribute is missing", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  # segID is not present in MixedConifer.las and we do not add it.
  pipeline <- reader_las() + li2012(store_in_attribute = "segID")
  expect_error(
    exec(pipeline, on = f),
    regexp = "segID is not present in the point cloud"
  )
})

test_that("li2012 errors when store_in_attribute has wrong type", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  # Declare segID as float instead of int -> should be rejected.
  addid <- add_extrabytes("float", "segID", "wrong type")
  pipeline <- reader_las() + addid + li2012(store_in_attribute = "segID")
  expect_error(
    exec(pipeline, on = f),
    regexp = "segID must be of type 'int' with scale=1 and offset=0"
  )
})

test_that("li2012 errors when store_in_attribute has wrong scale", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "wrong scale", scale = 0.01)
  pipeline <- reader_las() + addid + li2012(store_in_attribute = "segID")
  expect_error(
    exec(pipeline, on = f),
    regexp = "segID must be of type 'int' with scale=1 and offset=0"
  )
})

# Read per-point data back into R via lasR's own callback idiom.
# expose="*" surfaces all attributes (incl. custom extrabytes) by name.
read_points <- function(path, expose = "*") {
  identity_cb <- function(data) data
  exec(callback(identity_cb, expose = expose, no_las_update = TRUE),
       on = path)
}

test_that("li2012 yields all-NA when hmin exceeds the highest point", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  seg <- li2012(hmin = 1e6, store_in_attribute = "segID")  # above tallest point
  out_path <- tempfile(fileext = ".laz")
  wlas <- write_las(ofile = out_path)
  # The stage warns via REprintf (stderr), not an R condition, so we assert
  # the observable contract: with hmin above the highest point, no tree is
  # segmented and every point is NA.
  exec(reader_las() + addid + seg + wlas, on = f)
  d <- read_points(out_path)
  expect_true(all(is.na(d$segID)))
})

test_that("li2012 produces non-NA tree IDs on MixedConifer", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  seg <- li2012(store_in_attribute = "segID")
  out_path <- tempfile(fileext = ".laz")
  wlas <- write_las(ofile = out_path)
  exec(reader_las() + addid + seg + wlas, on = f)
  d <- read_points(out_path)
  expect_gt(sum(!is.na(d$segID)), 0)
  ids <- na.omit(d$segID)
  expect_true(all(ids > 0))          # tree IDs are positive (register_apex pre-increments from 0)
  expect_true(all(ids == as.integer(ids)))
})

test_that("li2012 matches lidR::li2012 modulo relabeling", {
  skip_if_not_installed("lidR")
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")

  # lasR side: filter at the READER (matching lidR readLAS(filter=)) so dropped
  # points never reach li2012. Fresh segID attribute (file already has treeID).
  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  rdr <- reader_las(filter = "-drop_class 7 18 -drop_withheld")
  seg <- li2012(store_in_attribute = "segID")
  out_path <- tempfile(fileext = ".laz")
  wlas <- write_las(ofile = out_path)
  exec(rdr + addid + seg + wlas, on = f)
  lasr_d <- read_points(out_path, expose = "*")

  ll <- eval(parse(text = paste0(
    'lidR::segment_trees(',
    '  lidR::readLAS("', f, '", filter = "-drop_class 7 18 -drop_withheld",',
    '    select = "xyz"),',
    '  lidR::li2012())')))
  lidr_d <- data.frame(X = ll@data$X, Y = ll@data$Y, Z = ll@data$Z,
                       lidr_id = ll@data$treeID)

  lasr_df <- data.frame(X = lasr_d$X, Y = lasr_d$Y, Z = lasr_d$Z,
                        lasr_id = lasr_d$segID)
  # Join on row index: the fixture has two points with identical XYZ (rows 1447
  # & 26870); an XYZ-only merge would cross-join them (2x2=4 rows). Both
  # implementations preserve the same point order, so row index is safe.
  expect_equal(nrow(lasr_df), nrow(lidr_d))
  expect_true(all(lasr_df$X == lidr_d$X & lasr_df$Y == lidr_d$Y & lasr_df$Z == lidr_d$Z),
              label = "point order must align before the row-index join")
  lasr_df$row <- seq_len(nrow(lasr_df))
  lidr_d$row  <- seq_len(nrow(lidr_d))
  joined <- merge(lasr_df, lidr_d, by = "row")
  expect_equal(nrow(joined), nrow(lasr_df))

  na_match <- is.na(joined$lasr_id) == is.na(joined$lidr_id)
  expect_true(all(na_match))

  both <- joined[!is.na(joined$lasr_id), ]
  tab <- table(both$lasr_id, both$lidr_id)
  # Every lasR tree maps to exactly one lidR tree and vice versa.
  expect_true(all(rowSums(tab > 0) == 1))
  expect_true(all(colSums(tab > 0) == 1))
})

test_that("li2012 matches lidR across parameter combinations", {
  skip_if_not_installed("lidR")
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  cases <- list(
    list(dt1 = 1.4, dt2 = 2.0, speed_up = 10),
    list(dt1 = 1.5, dt2 = 2.5, speed_up = 5),
    list(dt1 = 2.0, dt2 = 3.0, speed_up = 8)
  )
  for (cc in cases) {
    addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
    seg <- li2012(dt1 = cc$dt1, dt2 = cc$dt2, speed_up = cc$speed_up,
                  store_in_attribute = "segID")
    out_path <- tempfile(fileext = ".laz")
    wlas <- write_las(ofile = out_path)
    exec(reader_las() + addid + seg + wlas, on = f)
    lasr_d <- read_points(out_path, expose = "*")

    ll <- eval(parse(text = sprintf(
      'lidR::segment_trees(
         lidR::readLAS("%s", select = "xyz"),
         lidR::li2012(dt1 = %g, dt2 = %g, speed_up = %g))',
      f, cc$dt1, cc$dt2, cc$speed_up)))
    lidr_d <- data.frame(X = ll@data$X, Y = ll@data$Y, Z = ll@data$Z,
                         lidr_id = ll@data$treeID)

    lasr_df <- data.frame(X = lasr_d$X, Y = lasr_d$Y, Z = lasr_d$Z,
                          lasr_id = lasr_d$segID)
    # Row-index join: MixedConifer has a duplicate XYZ pair; same order in both.
    expect_equal(nrow(lasr_df), nrow(lidr_d))
    expect_true(all(lasr_df$X == lidr_d$X & lasr_df$Y == lidr_d$Y & lasr_df$Z == lidr_d$Z),
                label = "point order must align before the row-index join")
    lasr_df$row <- seq_len(nrow(lasr_df))
    lidr_d$row  <- seq_len(nrow(lidr_d))
    joined <- merge(lasr_df, lidr_d, by = "row")
    expect_equal(nrow(joined), nrow(lasr_df),
                 info = sprintf("Row drift dt1=%g dt2=%g su=%g",
                                cc$dt1, cc$dt2, cc$speed_up))
    expect_true(all(is.na(joined$lasr_id) == is.na(joined$lidr_id)),
                info = sprintf("NA mismatch dt1=%g dt2=%g su=%g",
                               cc$dt1, cc$dt2, cc$speed_up))
    both <- joined[!is.na(joined$lasr_id), ]
    tab <- table(both$lasr_id, both$lidr_id)
    expect_true(all(rowSums(tab > 0) == 1) && all(colSums(tab > 0) == 1),
                info = sprintf("Tree mapping not 1:1 dt1=%g dt2=%g",
                               cc$dt1, cc$dt2))
  }
})

test_that("li2012(R = 0) matches lidR::li2012(R = 0)", {
  skip_if_not_installed("lidR")
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  seg <- li2012(R = 0, store_in_attribute = "segID")
  out_path <- tempfile(fileext = ".laz")
  wlas <- write_las(ofile = out_path)
  exec(reader_las() + addid + seg + wlas, on = f)
  lasr_d <- read_points(out_path, expose = "*")

  ll <- eval(parse(text = sprintf(
    'lidR::segment_trees(
       lidR::readLAS("%s", select = "xyz"),
       lidR::li2012(R = 0))', f)))
  lidr_d <- data.frame(X = ll@data$X, Y = ll@data$Y, Z = ll@data$Z,
                       lidr_id = ll@data$treeID)

  lasr_df <- data.frame(X = lasr_d$X, Y = lasr_d$Y, Z = lasr_d$Z,
                        lasr_id = lasr_d$segID)
  # Row-index join: MixedConifer has a duplicate XYZ pair; same order in both.
  expect_equal(nrow(lasr_df), nrow(lidr_d))
  expect_true(all(lasr_df$X == lidr_d$X & lasr_df$Y == lidr_d$Y & lasr_df$Z == lidr_d$Z),
              label = "point order must align before the row-index join")
  lasr_df$row <- seq_len(nrow(lasr_df))
  lidr_d$row  <- seq_len(nrow(lidr_d))
  joined <- merge(lasr_df, lidr_d, by = "row")
  expect_equal(nrow(joined), nrow(lasr_df))
  expect_true(all(is.na(joined$lasr_id) == is.na(joined$lidr_id)))
  both <- joined[!is.na(joined$lasr_id), ]
  tab <- table(both$lasr_id, both$lidr_id)
  expect_true(all(rowSums(tab > 0) == 1) && all(colSums(tab > 0) == 1))
})

test_that("li2012 with filter leaves filtered points as NA", {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  seg <- li2012(filter = "-keep_first", store_in_attribute = "segID")
  out_path <- tempfile(fileext = ".laz")
  wlas <- write_las(ofile = out_path)
  exec(reader_las() + addid + seg + wlas, on = f)
  d <- read_points(out_path, expose = "*")
  first <- d$ReturnNumber == 1L
  expect_true(all(is.na(d$segID[!first])))   # non-first returns must be NA
  expect_gt(sum(!is.na(d$segID[first])), 0)  # some first returns got an ID
})

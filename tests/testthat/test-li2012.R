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

# Build four equal quadrant tiles from the bundled fixture. The engine adds the
# stage-declared buffer halo from neighbouring tiles automatically at exec time.
#
# reader_rectangles uses closed [xmin,xmax]x[ymin,ymax] intervals, so if the
# midpoint lands exactly on the data grid (as it does for MixedConifer.las
# where ymid=3812966.04 is an exact 0.01-scale grid point), points at exactly
# ymid would appear in both the lower and upper tiles.  We avoid that by
# shifting the lower tiles's upper Y boundary half a grid step (0.005) below
# ymid.  No real data point falls at that gap, so the partition is lossless
# and non-overlapping.
make_quadrant_tiles <- function(dir) {
  f <- system.file("extdata", "MixedConifer.las", package = "lasR")
  .bbox_env <- new.env(parent = emptyenv())
  bbox_cb <- function(data) {
    .bbox_env$bb <- c(min(data$X), max(data$X), min(data$Y), max(data$Y))
    invisible(NULL)
  }
  exec(callback(bbox_cb, expose = "xy", no_las_update = TRUE), on = f)
  bb <- .bbox_env$bb
  xmin <- bb[1]; xmax <- bb[2]; ymin <- bb[3]; ymax <- bb[4]
  xmid <- (xmin + xmax) / 2
  ymid <- (ymin + ymax) / 2
  # Half a grid step below ymid so the closed-interval split is non-overlapping.
  ymid_lo <- ymid - 0.005

  quads <- list(
    list(name = "q_ll.laz", xmin = xmin, ymin = ymin,  xmax = xmid, ymax = ymid_lo),
    list(name = "q_lr.laz", xmin = xmid, ymin = ymin,  xmax = xmax, ymax = ymid_lo),
    list(name = "q_ul.laz", xmin = xmin, ymin = ymid,  xmax = xmid, ymax = ymax),
    list(name = "q_ur.laz", xmin = xmid, ymin = ymid,  xmax = xmax, ymax = ymax))

  paths <- character(4)
  for (i in seq_along(quads)) {
    q <- quads[[i]]
    paths[i] <- file.path(dir, q$name)
    exec(reader_rectangles(q$xmin, q$ymin, q$xmax, q$ymax) +
         write_las(ofile = paths[i]), on = f)
  }
  paths
}

test_that("li2012 produces globally-consistent IDs across 4 concurrent tiles", {
  skip_if_not(has_omp_support())

  tiledir <- file.path(tempdir(), "li2012_seam")
  dir.create(tiledir, showWarnings = FALSE, recursive = TRUE)
  paths <- make_quadrant_tiles(tiledir)

  outdir <- file.path(tempdir(), "li2012_seam_out")
  dir.create(outdir, showWarnings = FALSE)
  unlink(list.files(outdir, full.names = TRUE))

  addid <- add_extrabytes("int", "segID", "Li 2012 tree ID")
  seg <- li2012(store_in_attribute = "segID")
  wlas <- write_las(ofile = file.path(outdir, "{*}.laz"))
  exec(reader_las() + addid + seg + wlas,
       on = paths, ncores = concurrent_files(4L))
  all_pts <- do.call(rbind, lapply(
    list.files(outdir, full.names = TRUE), read_points))

  # Single-tile baseline.
  baseline_path <- tempfile(fileext = ".laz")
  exec(reader_las() + add_extrabytes("int", "segID", "Li 2012 tree ID") +
       li2012(store_in_attribute = "segID") +
       write_las(ofile = baseline_path),
       on = system.file("extdata", "MixedConifer.las", package = "lasR"))
  base <- read_points(baseline_path)

  # (1) THE cross-tile guarantee: apex-keyed dedup yields globally-consistent
  # IDs. `reader_rectangles` is boundary-inclusive, so a few seam points are
  # written by more than one tile; every copy of such a physical point MUST
  # carry the same tree ID -- i.e. no tree is split into two IDs across a seam.
  # This is the property concurrent-files must satisfy, and we assert it strictly.
  key <- paste(all_pts$X, all_pts$Y, all_pts$Z)
  dup_keys <- unique(key[duplicated(key)])
  inconsistent <- vapply(dup_keys, function(k) {
    ids <- all_pts$segID[key == k]
    length(unique(ids[!is.na(ids)])) > 1
  }, logical(1))
  expect_equal(sum(inconsistent), 0L)

  # Deduplicate physical points (IDs just verified consistent) for comparison.
  uniq <- all_pts[!duplicated(key), ]

  # (2) Tree count matches the single-tile baseline within a small tolerance.
  # (Measured delta on this fixture is 0.)
  nt_tiled <- length(unique(na.omit(uniq$segID)))
  nt_base  <- length(unique(na.omit(base$segID)))
  expect_lte(abs(nt_tiled - nt_base), 2L)

  # (3) The partition closely matches single-tile, modulo relabeling. Exact
  # per-point equivalence is NOT achievable for tiled greedy segmentation: a
  # point's fate near a seam can depend on an apex-suppression cascade that
  # exceeds any fixed buffer (lidR's catalog segment_trees does not guarantee
  # bit-exact tiling either). So we assert high agreement, not a strict
  # bijection. Measured on this fixture: ~98.3% partition agreement, ~99.8%
  # NA-status agreement.
  joined <- merge(uniq[, c("X", "Y", "Z", "segID")],
                  base[, c("X", "Y", "Z", "segID")],
                  by = c("X", "Y", "Z"), suffixes = c("_par", "_base"))
  expect_gt(mean(is.na(joined$segID_par) == is.na(joined$segID_base)), 0.98)
  both <- joined[!is.na(joined$segID_par) & !is.na(joined$segID_base), ]
  tab <- table(both$segID_par, both$segID_base)
  agreement <- sum(apply(tab, 1, max)) / sum(tab)
  expect_gt(agreement, 0.95)
})

test_that("li2012 partition is invariant to concurrent-files count", {
  skip_if_not(has_omp_support())

  tiledir <- file.path(tempdir(), "li2012_order")
  dir.create(tiledir, showWarnings = FALSE, recursive = TRUE)
  paths <- make_quadrant_tiles(tiledir)

  run_at <- function(ncores) {
    outdir <- file.path(tempdir(), paste0("li2012_order_", ncores))
    dir.create(outdir, showWarnings = FALSE)
    unlink(list.files(outdir, full.names = TRUE))
    exec(reader_las() + add_extrabytes("int", "segID", "Li 2012 tree ID") +
         li2012(store_in_attribute = "segID") +
         write_las(ofile = file.path(outdir, "{*}.laz")),
         on = paths, ncores = concurrent_files(ncores))
    pts <- do.call(rbind, lapply(
      list.files(outdir, full.names = TRUE), read_points))
    pts[!duplicated(paste(pts$X, pts$Y, pts$Z)), ]
  }

  pts1 <- run_at(1L)
  pts4 <- run_at(4L)

  # Same tiling at different thread counts must yield the IDENTICAL partition
  # modulo relabeling: each tile's growth is deterministic and the apex dedup is
  # order-independent. This is exact (unlike the single-tile comparison above).
  m <- merge(pts1[, c("X", "Y", "Z", "segID")],
             pts4[, c("X", "Y", "Z", "segID")],
             by = c("X", "Y", "Z"), suffixes = c("_1", "_4"))
  expect_equal(nrow(m), nrow(pts1))
  expect_true(all(is.na(m$segID_1) == is.na(m$segID_4)))
  both <- m[!is.na(m$segID_1), ]
  tab <- table(both$segID_1, both$segID_4)
  expect_true(all(rowSums(tab > 0) == 1) && all(colSums(tab > 0) == 1))
})

test_that("li2012 declares need_buffer = 2*speed_up + R/2", {
  # get_pipeline_info is internal (not in NAMESPACE), so use ::: .
  p <- reader_las() + li2012(speed_up = 10, R = 2)
  info <- lasR:::get_pipeline_info(p)
  # The declared buffer should be 2*10 + 2/2 = 21.
  expect_gte(info$buffer, 21)
})

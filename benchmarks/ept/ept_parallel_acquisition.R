#!/usr/bin/env Rscript
# Benchmark: parallel vs sequential EPT acquisition on a remote (S3) endpoint.
#
# Endpoint:  USGS 3DEP AZ_BrawleyRillito_TL_2018 (Tucson area, ~2B points,
#            native EPSG:3857 — no reprojection overhead).
# AOI:       10,000 acres (6,361 m x 6,361 m), centered inside the
#            dataset's boundsConforming.
# Pipeline:  reader_rectangles(AOI) + summarise()
# Strategies: sequential, concurrent_files(2/4/8/16)
#
# Live progress is printed for every config; live progress for each
# strategy is also enabled via progress = TRUE inside exec().

suppressMessages(library(lasR))

stopifnot(has_omp_support())

URL <- "https://s3-us-west-2.amazonaws.com/usgs-lidar-public/AZ_BrawleyRillito_TL_2018/ept.json"

# boundsConforming: [-12430045, 3703536, -12396617, 3813708]
# Center inside the conformal box. 10,000 acres ≈ 40,468,564 m^2
# → side 6,361.5 m → half 3,180.7 m.
cx <- (-12430045 + -12396617) / 2   # -12413331
cy <- (3703536 + 3813708) / 2       #   3758622
h  <- 3180.5
xmin <- cx - h; xmax <- cx + h
ymin <- cy - h; ymax <- cy + h

cat(sprintf("Endpoint: %s\n", URL))
cat(sprintf("AOI (EPSG:3857): [%.1f, %.1f, %.1f, %.1f]\n", xmin, ymin, xmax, ymax))
cat(sprintf("AOI area: %.1f km^2  (%.0f acres)\n",
            (xmax-xmin)*(ymax-ymin)/1e6,
            (xmax-xmin)*(ymax-ymin) / 4046.8564224))
cat(sprintf("Host CPUs: %d\n", parallel::detectCores()))

q <- reader_rectangles(xmin, ymin, xmax, ymax) + summarise()

time_run <- function(label, strategy, i = NA, n = NA) {
  gc(full = TRUE)
  cat(sprintf("  [%d/%d] %-26s starting ...\n", i, n, label)); flush.console()
  t0 <- Sys.time()
  ans <- exec(q, on = URL, ncores = strategy)
  dt <- as.numeric(difftime(Sys.time(), t0, units = "secs"))
  cat(sprintf("  [%d/%d] %-26s npoints=%-12d  wall=%7.2f s  mpts/s=%6.2f\n",
              i, n, label, ans$npoints, dt, ans$npoints / dt / 1e6))
  flush.console()
  list(label = label, npoints = ans$npoints, wall = dt)
}

# --- Warm-up: prime DNS / TCP / metadata cache so seq run is fair. ----------
cat("\n[warm-up: small depth=0 read to prime caches]\n")
invisible(exec(reader(depth = 0) + summarise(), on = URL, ncores = sequential()))

# --- Run sweep --------------------------------------------------------------
# Two trials per config so we can spot S3-throttling / cold-cache variance.
configs <- list(
  list(label = "sequential",            strategy = sequential()),
  list(label = "concurrent_files(2)",   strategy = concurrent_files(2)),
  list(label = "concurrent_files(4)",   strategy = concurrent_files(4)),
  list(label = "concurrent_files(8)",   strategy = concurrent_files(8)),
  list(label = "concurrent_files(16)",  strategy = concurrent_files(16))
)

cat("\n[runs]\n")
results <- vector("list", length(configs))
for (i in seq_along(configs)) {
  results[[i]] <- time_run(configs[[i]]$label, configs[[i]]$strategy,
                           i = i, n = length(configs))
}

# --- Sanity check: all strategies must return the same npoints. -------------
ns <- vapply(results, function(r) as.numeric(r$npoints), numeric(1))
stopifnot(length(unique(ns)) == 1L)
baseline <- results[[1]]$wall
cat("\n[summary]  (baseline = sequential)\n")
cat(sprintf("  %-26s  %-9s  %-9s  %-9s\n", "strategy", "wall(s)", "speedup", "eff%"))
for (i in seq_along(results)) {
  r <- results[[i]]
  cores <- if (i == 1) 1 else c(2, 4, 8, 16)[i - 1]
  sp <- baseline / r$wall
  eff <- 100 * sp / cores
  cat(sprintf("  %-26s  %9.2f  %9.2fx  %8.1f%%\n",
              r$label, r$wall, sp, eff))
}

cat(sprintf("\nAll runs returned npoints = %d  (correctness OK)\n", ns[1]))

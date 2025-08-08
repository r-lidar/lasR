library(lasR)

f = c("/home/jr/Documents/Ulaval/ALS data/BCTS/092L072244_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS/092L072422_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS/092L073133_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS/092L073311_BCTS_2.laz")

f = paste0(system.file(package="lasR"), "/extdata/bcts/")
f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

normalize_pileline = reader() + normalize() + write_las()
dtm_dsm_pipeline = reader() + dtm() + chm()

dsm = chm()
chm = pit_fill(dsm)
tree = local_maximum(3)
segment = region_growing(chm, tree)
segmentation_pipeline = reader() + normalize() + dsm + chm + tree + segment

pre_read = reader() + lasR:::nothing(stream = T)

# ============
# PRE-READ
# ============

# ensure that the files are already in cache to ensure fair
# comparison of all pipelines
exec(pre_read, on = f, ncores = 4, progress=T)

#=============
# PIPELINE 1
# ============

t0 = Sys.time()
exec(normalize_pileline, on = f, ncores = 4, progress=T)
Sys.time() - t0
# 1'12"

t0 = Sys.time()
exec(normalize_pileline, on = f, ncores = concurrent_points(8), progress = T)
Sys.time() - t0
# 0'50"

t0 = Sys.time()
exec(normalize_pileline, on = f,  ncores = concurrent_files(4), progress = T)
Sys.time() - t0
# 0'32"

# =============
# PIPELINE 2
# =============

t0 = Sys.time()
exec(dtm_dsm_pipeline, on = f, ncores = 1, progress = T)
Sys.time() - t0
# 0'41"

t0 = Sys.time()
exec(dtm_dsm_pipeline, on = f, ncores = concurrent_points(8), progress = T)
Sys.time() - t0
# 0'40" (there is no parallelized stage is this pipeline)

t0 = Sys.time()
exec(dtm_dsm_pipeline, on = f, ncores = concurrent_files(4), progress = T)
Sys.time() - t0
# 0'18"

# =============
# PIPELINE 3
# =============

t0 = Sys.time()
exec(segmentation_pipeline, on = f, ncores = 1, progress = T)
Sys.time() - t0
# 2'21"

t0 = Sys.time()
exec(segmentation_pipeline, on = f, ncores = concurrent_points(8), progress = T)
Sys.time() - t0
# 1'48"

t0 = Sys.time()
exec(segmentation_pipeline, on = f, ncores = concurrent_files(4), progress = T)
Sys.time() - t0
# 1'5"

q("no")

ans = processor(segmentation_pipeline, ncores = 1, mode = concurrent_files())
ans

terra::plot(ans$rasterize)

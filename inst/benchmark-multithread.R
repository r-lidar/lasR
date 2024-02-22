library(lasR)

f = c("/home/jr/Documents/Ulaval/ALS data/BCTS/092L072244_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS/092L072422_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS/092L073133_BCTS_2.laz",
      "/home/jr/Documents/Ulaval/ALS data/BCTS/092L073311_BCTS_2.laz")

f = paste0(system.file(package="lasR"), "/extdata/bcts/")
f = list.files(f, pattern = "(?i)\\.la(s|z)$", full.names = TRUE)

normalize_pileline = reader(f) + normalize() + write_las()
dtm_dsm_pipeline = reader(f) + dtm() + chm()

dsm = chm()
chm = pit_fill(dsm)
tree = local_maximum(3)
segment = region_growing(chm, tree)
segmentation_pipeline = reader(f) + normalize() + dsm + chm + tree + segment

pre_read = reader(f) + lasR:::nothing(stream = T)

# ============
# PRE-READ
# ============

# ensure that the files are already in cache to ensure fair
# comparison of all pipelines
processor(pre_read)

#=============
# PIPELINE 1
# ============

t0 = Sys.time()
processor(normalize_pileline, ncores = 1)
Sys.time() - t0
# 1'12"

t0 = Sys.time()
processor(normalize_pileline, ncores = 8, mode = concurrent_points())
Sys.time() - t0
# 0'50"

t0 = Sys.time()
processor(normalize_pileline, ncores = 4, mode = concurrent_files())
Sys.time() - t0
# 0'32"

# =============
# PIPELINE 2
# =============

t0 = Sys.time()
processor(dtm_dsm_pipeline, ncores = 1)
Sys.time() - t0
# 0'41"

t0 = Sys.time()
processor(dtm_dsm_pipeline, ncores = 8, mode = concurrent_points())
Sys.time() - t0
# 0'40" (there is no parallelized stage is this pipeline)

t0 = Sys.time()
processor(dtm_dsm_pipeline, ncores = 4, mode = concurrent_files())
Sys.time() - t0
# 0'18"

# =============
# PIPELINE 3
# =============

t0 = Sys.time()
processor(segmentation_pipeline, ncores = 1)
Sys.time() - t0
# 2'21"

t0 = Sys.time()
processor(segmentation_pipeline, ncores = 8, mode = concurrent_points())
Sys.time() - t0
# 1'48"

t0 = Sys.time()
processor(segmentation_pipeline, ncores = 4, mode = concurrent_files())
Sys.time() - t0
# 1'5"

q("no")

ans = processor(segmentation_pipeline, ncores = 1, mode = concurrent_files())
ans

terra::plot(ans$rasterize)

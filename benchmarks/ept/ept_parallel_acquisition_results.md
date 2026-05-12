# Parallel remote-EPT acquisition: bulk-prefetch tile bodies to `/vsimem/`

## Summary

The `readers.ept` path in lasR now overlaps tile downloads with LASzip decode by bulk-fetching each prefetched tile into GDAL's in-memory filesystem (`/vsimem/`) on the worker thread. The consumer's `read_point()` runs entirely against RAM. This is the same trick PDAL's `readers.ept` uses internally with `requests=N`, now done per-worker in lasR.

On a real remote EPT — USGS 3DEP `AZ_BrawleyRillito_TL_2018` (2.0 B points, native EPSG:3857) — a 10,000-acre AOI (58,636,837 points returned) is now **1.1×–1.6× faster than PDAL** at every concurrency level we tested.

## Benchmark

- **Endpoint:** `s3-us-west-2.amazonaws.com/usgs-lidar-public/AZ_BrawleyRillito_TL_2018/ept.json`
- **AOI:** 10,000 acres (40.5 km², 6,361 m × 6,361 m), inside `boundsConforming`
- **Pipeline:** `reader_rectangles(AOI) + summarise()` (lasR) and `readers.ept + filters.stats` (PDAL 2.10.1)
- **Host:** 16 vCPUs
- **Returned points:** 58,636,837 in every run — identical across lasR and PDAL single-process modes (correctness gate)

### lasR

| strategy             | wall (s) | speedup | parallel eff. | Mpts/s |
|----------------------|---------:|--------:|--------------:|-------:|
| sequential           |    65.6  |   1.00× |        100.0% |  0.89 |
| concurrent_files(2)  |    48.4  |   1.35× |         67.7% |  1.21 |
| concurrent_files(4)  |    33.0  |   1.99× |         49.7% |  1.78 |
| concurrent_files(8)  |    31.0  |   2.12× |         26.4% |  1.89 |
| concurrent_files(16) |    27.5  |   2.38× |         14.9% |  2.13 |

Outer-parallelism efficiency drops past `concurrent_files(4)` because the AWS S3 outbound pipe to this host saturates at ~4 concurrent fetches — PDAL hits the same wall (see below).

### PDAL (same AOI, same endpoint)

| mode                                | wall (s) | speedup vs `requests=1` |
|-------------------------------------|---------:|------------------------:|
| `requests=1`                        |   104.1  | 1.00× |
| `requests=2`                        |    53.0  | 1.96× |
| `requests=4`                        |    44.9  | 2.32× |
| `requests=8`                        |    45.4  | 2.29× |
| `requests=16`                       |    45.1  | 2.31× |
| 16 procs × `requests=1` (4×4 grid)  |    42.7  | 2.44× |

The 16-process PDAL variant returns 58,636,867 points — 30 extra. Each subprocess clips to its own sub-AOI without lasR's strict-clip ownership protocol, so a handful of grid-seam points get double-counted.

### Head-to-head

| concurrency | PDAL  | lasR  | lasR vs PDAL |
|------------:|------:|------:|-------------:|
|  1          | 104 s | **66 s** | **1.6× faster** |
|  2          |  53 s | **48 s** | **1.1× faster** |
|  4          |  45 s | **33 s** | **1.4× faster** |
|  8          |  45 s | **31 s** | **1.5× faster** |
| 16          |  43 s | **27 s** | **1.6× faster** |

## What changed

Two files, ~100-line diff.

**`src/LASRreaders/EPTio.{h,cpp}`**
- Added a deque-based prefetch queue and `int prefetch_depth` (default 4, overridable via `LASR_EPT_PREFETCH`, clamped [1, 32]). Replaces the single `next_tile_future`.
- `open_tile_sync()` for **remote** tiles now:
  1. `VSIFOpenL("/vsicurl/<tile>.laz")`
  2. `VSIFReadL` the full body into a `CPLMalloc` buffer
  3. Registers it under a unique `/vsimem/lasR_ept/<this>_<d-x-y-z>_<seq>.laz` via `VSIFileFromMemBuffer(..., bTakeOwnership=TRUE)`
  4. Opens `LASio` against the `/vsimem/` path
- `TileLoadResult` carries the `/vsimem/` key. `cancel_prefetch`, `open_next_tile`, and `close` all `VSIUnlink` it to avoid leaking GDAL in-memory buffers. Local-file path is unchanged.

**`src/vendor/LASzip/mydefs.cpp`**
- One line: `is_remote_path()` now recognizes `/vsimem/`. Without this, `LASreaderLAS::open` falls through to `fopen()` for `/vsimem/` paths (libc can't see GDAL's virtual FS) instead of routing through `VSIFOpenL`.

## Correctness

- All 37 cases in `tests/testthat/test-ept.R` pass.
- Every benchmark strategy returns identical `npoints = 58,636,837` — sequential, all `concurrent_files(N)` levels, and PDAL single-process at every `requests` setting.

## Why this works

Without pre-buffering, the existing async prefetch only opened the tile header (~few KB) in the worker; the bulk LAZ bytes were still pulled on-demand by the consumer thread, serialized with LASzip decode. Deepening that header-only prefetch from 1 to 4 saved only 2–13% across the sweep.

The fix moves the **full** download off the consumer thread. While the consumer is decoding tile N from RAM, tiles N+1…N+4's bytes are streaming into `/vsimem/` in parallel via four `VSIFReadL` workers. AWS S3 is HTTP/1.1-only — connection multiplexing is unavailable on this endpoint, so adding concurrent TCP connections is the only way to overlap I/O. PDAL gets this via its libcurl multi-handle pool (`requests=15` default); lasR now gets it via std::async + `/vsimem/` per worker.

## Tunables and footprint

- `LASR_EPT_PREFETCH` (env var, default 4) — number of tiles to keep in flight per `EPTio` instance.
- Transient memory: `prefetch_depth × outer_threads × tile_size`. At `concurrent_files(16) × 4-deep × ~few-MB EPT tiles` this is typically well under 500 MB.
- Default of 4 was chosen because PDAL plateaus at `requests=4` on this host. Different network profiles may want a different default; the env var is the knob.

## Follow-ups (not in this PR)

- One redundant memcpy in `open_tile_sync` (the bytes land in a `std::vector` first, then a `CPLMalloc` copy for `VSIFileFromMemBuffer`'s ownership semantics). Switching to a single `CPLMalloc` and reading directly into it would save a copy per tile. Cheap relative to network, but cleaner.
- Per-tile `/vsimem/` registration is per-process; for very long-running R sessions, the unique-suffix sequence is a `static std::atomic<uint64_t>` — fine in practice but technically unbounded. Could be reset per `EPTio` instance.

## Reproducing

```sh
# lasR
Rscript benchmarks/ept_parallel_acquisition.R

# PDAL
bash benchmarks/ept_pdal_compare.sh   # or .py
```

Both write live per-run progress and finish with a summary table; the lasR script asserts identical `npoints` across all strategies as a correctness gate.

# COPC writer benchmarks

This harness benchmarks lasR's COPC writer against PDAL, Untwine, and
LAStools `lascopcindex`, then measures HTTP-range read performance of each
writer's output through lasR's reader.

## Prerequisites

Tools must be on `$PATH`:

- PDAL 2.9+ — `apt install pdal` or build from source.
- Untwine 1.5+ — `conda install -c conda-forge untwine`, or build from
  `github.com/hobuinc/untwine`.
- `lascopcindex` and `lasinfo` from LAStools — build from
  `github.com/LAStools/LAStools` (`cmake . && make`), copy `bin64/lascopcindex64`
  to `~/.local/bin/lascopcindex` and `bin64/lasinfo64` to `~/.local/bin/lasinfo`.
- R 4.3+ with `lasR`, `httpuv`, `jsonlite`. Install lasR from the current branch:
  `R CMD INSTALL .` from the repo root before running the benchmark.

## Usage

`benchmarks/copc/run_all.sh` fetches input, runs writers, runs reads, builds
report. Outputs land in `benchmarks/copc/results/`.

`lasR-default` is the legacy LASlib writer run at density `"normal"`, the same
as `lasR-experimental`, while the legacy writer's own default is `"dense"`.

`benchmarks/copc/run_multitile_validation.sh` renders per-depth density rasters
of the single-tile output, merges the 2×2 tile block into one COPC and checks
the tile seams, for every point and per octree depth. It needs `terra`.

`Rscript check_z_plane_bands.R <copc> [res_m] [max_depth]` reports whether the
denser cells of the coarse levels are the ones whose point column crosses an
octree cell boundary in z.

## Reports

- `REPORT-2026-09-30.md` — write and read benchmark of the six writers.
- `REPORT-LOD-MULTITILE-2026-09-30.md` — LOD quality, multi-tile merge and
  tile-boundary check.

`results/` is not committed. The figures the reports use are kept in
`figures/`.

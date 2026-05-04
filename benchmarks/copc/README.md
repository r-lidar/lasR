# COPC writer benchmarks

This harness benchmarks lasR's COPC writer against PDAL, Untwine, and
LAStools `lascopcindex`, then measures HTTP-range read performance of each
writer's output through lasR's reader. See the design spec under
`docs/superpowers/specs/`.

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

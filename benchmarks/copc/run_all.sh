#!/usr/bin/env bash
# End-to-end COPC writer benchmark.
set -euo pipefail
cd "$(dirname "$0")"

WARM_REPEATS=${WARM_REPEATS:-3}
DROP_CACHES=${DROP_CACHES:-auto}

mkdir -p results

echo "=== fetch ==="
./fetch_dataset.sh

echo "=== versions ==="
{
  echo "PDAL:"
  pdal --version 2>&1 || true
  echo
  echo "Untwine:"
  untwine --version 2>&1 || true
  echo
  echo "lascopcindex:"
  lascopcindex -version 2>&1 | head -3 || true
  echo
  echo "lasinfo:"
  lasinfo -version 2>&1 | head -3 || true
  echo
  echo "lasR:"
  Rscript -e 'cat(as.character(packageVersion("lasR")))' 2>&1
  echo
  echo "R:"
  R --version | head -2
} > results/versions.txt

echo "=== write phase ==="
WARM_REPEATS="$WARM_REPEATS" DROP_CACHES="$DROP_CACHES" ./run_writers.sh

echo "=== read phase (M2) ==="
./run_reads.sh

if [[ -n "${LASR_BENCH_S3_BASE:-}" ]]; then
  echo "[run_all] LASR_BENCH_S3_BASE is set, but M3 is deferred until S3 byte/request logging is implemented"
fi

echo "=== report ==="
Rscript make_report.R

echo "[run_all] done - see results/REPORT.md"

#!/usr/bin/env bash
# Orchestrates LOD visual validation + multi-tile merge + edge-artifact check
# for the lasR-experimental COPC writer.
#
# Usage: bash run_multitile_validation.sh [--skip-fetch] [--lod-res N] [--density normal|dense]
#
# Phases:
#   1  LOD visual validation on the existing single-tile lasR-experimental COPC
#   2  Fetch adjacent USGS 3DEP tiles
#   3  Write merged COPC from all fetched tiles
#   4  LOD visual validation on the merged COPC
#   5  Edge-artifact check on the merged COPC
set -euo pipefail
cd "$(dirname "$0")"

# ---- Parse args ---------------------------------------------------------------
SKIP_FETCH=false
LOD_RES=10
DENSITY=normal
while [[ $# -gt 0 ]]; do
  case $1 in
    --skip-fetch)  SKIP_FETCH=true; shift ;;
    --lod-res)     LOD_RES=$2; shift 2 ;;
    --density)     DENSITY=$2; shift 2 ;;
    *) echo "unknown arg: $1" >&2; exit 1 ;;
  esac
done

RES_DIR="results/multitile"
SINGLE_COPC="results/outputs/lasR-experimental.copc.laz"
TILES_DIR="data/tiles"
MERGED_COPC="$RES_DIR/merged_experimental.copc.laz"
mkdir -p "$RES_DIR"

# ---- Phase 1: LOD visual validation (single tile) ----------------------------
echo
echo "=== Phase 1: LOD visual validation — single tile ==="
if [[ -s "$SINGLE_COPC" ]]; then
  Rscript validate_lod_visual.R "$SINGLE_COPC" "$RES_DIR/lod_visual_single" "$LOD_RES"
else
  echo "[run] $SINGLE_COPC not found — run run_writers.sh first" >&2
fi

# ---- Phase 2: Fetch adjacent tiles -------------------------------------------
echo
echo "=== Phase 2: Fetch adjacent USGS tiles ==="
if [[ "$SKIP_FETCH" == "true" ]]; then
  echo "[run] --skip-fetch: using whatever is already in $TILES_DIR/"
else
  bash fetch_multitile.sh
fi

n_tiles=$(ls "$TILES_DIR/"*.laz 2>/dev/null | wc -l || echo 0)
echo "[run] $n_tiles tile(s) available in $TILES_DIR/"

# ---- Phase 3: Write merged COPC ----------------------------------------------
echo
echo "=== Phase 3: Write merged COPC (lasR-experimental, density=$DENSITY) ==="
if [[ $n_tiles -lt 2 ]]; then
  echo "[run] need ≥2 tiles for merge test; only $n_tiles found — skipping phases 3-5" >&2
  exit 0
fi

Rscript write_lasR_multitile.R "$TILES_DIR" "$MERGED_COPC" "$DENSITY"

merged_mb=$(du -m "$MERGED_COPC" | awk '{print $1}')
echo "[run] merged COPC: $MERGED_COPC (${merged_mb} MiB)"

echo "[run] LOD inspection of merged COPC:"
Rscript inspect_copc.R "merged_experimental" "$MERGED_COPC" \
        "$RES_DIR/lod_merged_experimental.csv"

# ---- Phase 4: LOD visual validation (merged COPC) ----------------------------
echo
echo "=== Phase 4: LOD visual validation — merged COPC ==="
Rscript validate_lod_visual.R "$MERGED_COPC" "$RES_DIR/lod_visual_merged" "$LOD_RES"

# ---- Phase 5: Edge-artifact check --------------------------------------------
echo
echo "=== Phase 5: Edge-artifact check ==="
edge_status=0
Rscript check_edge_artifacts.R "$MERGED_COPC" "$TILES_DIR" \
        "$RES_DIR/edge_check" "$LOD_RES" || edge_status=$?

# ---- Summary -----------------------------------------------------------------
echo
echo "=== Summary ==="
echo "Results directory: $RES_DIR/"
ls -lh "$RES_DIR/" 2>/dev/null

if [[ $edge_status -ne 0 ]]; then
  echo "RESULT: edge-artifact check flagged anomalies — review $RES_DIR/edge_check/"
  exit 1
else
  echo "RESULT: all phases passed"
fi

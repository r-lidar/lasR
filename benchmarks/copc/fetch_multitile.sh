#!/usr/bin/env bash
# Downloads adjacent USGS 3DEP tiles to data/tiles/ for multi-tile merge testing.
#
# Tile naming convention for CA_NorthCoastRanges_B23:
#   USGS_LPC_CA_NorthCoastRanges_B23_<XXXYYY>.laz
#   where XXX = floor(xmin / 1000) and YYY = floor(ymin / 1000) mod 1000
#   (e.g. anchor 502378 → xmin~502000, ymin~4378000)
#
# Tries E, N, NE, W, S neighbours; stops once anchor + MIN_ADJACENT are available.
set -euo pipefail
cd "$(dirname "$0")"

BASE_URL="https://rockyweb.usgs.gov/vdelivery/Datasets/Staged/Elevation/LPC/Projects/CA_NorthCoastRanges_B23/CA_NorthCoastRanges_1_B23/LAZ"
OUT_DIR="data/tiles"
ANCHOR_ID=502378
MIN_ADJACENT=${MIN_ADJACENT:-3}   # how many neighbours to collect beside anchor

mkdir -p "$OUT_DIR"

# ---- Seed anchor tile from data/input.laz ------------------------------------
anchor_fname="USGS_LPC_CA_NorthCoastRanges_B23_${ANCHOR_ID}.laz"
if [[ ! -s "$OUT_DIR/$anchor_fname" ]]; then
  if [[ -s "data/input.laz" ]]; then
    cp "data/input.laz" "$OUT_DIR/$anchor_fname"
    echo "[fetch_multitile] seeded anchor tile from data/input.laz"
  else
    echo "[fetch_multitile] WARNING: data/input.laz not found; anchor will be downloaded"
  fi
fi

# ---- Helper: attempt one tile download ---------------------------------------
download_tile() {
  local tile_id=$1
  local fname="USGS_LPC_CA_NorthCoastRanges_B23_${tile_id}.laz"
  local url="$BASE_URL/$fname"
  local out="$OUT_DIR/$fname"

  if [[ -s "$out" ]]; then
    echo "[fetch_multitile] already present: $fname"
    return 0
  fi

  # HEAD request to probe existence (2 retries, short timeout)
  local http_code
  http_code=$(curl -fsL --head -o /dev/null -w "%{http_code}" \
                   --connect-timeout 10 --retry 2 "$url" 2>/dev/null || echo "000")
  if [[ "$http_code" != "200" ]]; then
    echo "[fetch_multitile] tile $tile_id not available (HTTP $http_code)"
    return 1
  fi

  echo "[fetch_multitile] downloading $fname ..."
  curl -fSL --retry 3 --retry-delay 3 --connect-timeout 30 \
       -o "${out}.tmp" "$url"
  mv "${out}.tmp" "$out"
  local bytes
  bytes=$(stat -c %s "$out")
  echo "[fetch_multitile] OK: $fname ($bytes bytes)"
  return 0
}

# ---- Candidates: E, N, NE, W, S, SE, SW, NW ---------------------------------
# Derive neighbour IDs from anchor: XXX = 502, YYY = 378
AX=502; AY=378
CANDIDATES=(
  $(( AX+1 ))${AY}          # E
  ${AX}$(( AY+1 ))          # N
  $(( AX+1 ))$(( AY+1 ))    # NE
  $(( AX-1 ))${AY}          # W
  ${AX}$(( AY-1 ))          # S
  $(( AX+1 ))$(( AY-1 ))    # SE
  $(( AX-1 ))$(( AY+1 ))    # NW
  $(( AX-1 ))$(( AY-1 ))    # SW
)

succeeded=0
for tile_id in "${CANDIDATES[@]}"; do
  if download_tile "$tile_id"; then
    succeeded=$(( succeeded + 1 ))
    [[ $succeeded -ge $MIN_ADJACENT ]] && break
  fi
done

total=$(ls "$OUT_DIR/"*.laz 2>/dev/null | wc -l)
echo "[fetch_multitile] done: $total tile(s) in $OUT_DIR/ (anchor + $succeeded adjacent)"
ls -lh "$OUT_DIR/"*.laz 2>/dev/null

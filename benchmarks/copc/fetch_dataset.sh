#!/usr/bin/env bash
# Downloads the pinned 3DEP tile to data/input.laz and records its
# size, sha256, and parsed LAS header into results/run_metadata.txt.
set -euo pipefail

cd "$(dirname "$0")"

URL=$(tr -d '\r\n' < dataset.txt)
OUT=data/input.laz
META=results/run_metadata.txt

if [[ ! -s "$OUT" ]]; then
  echo "[fetch] downloading $URL"
  curl -fSL --retry 3 --retry-delay 2 -o "$OUT.tmp" "$URL"
  mv "$OUT.tmp" "$OUT"
fi

bytes=$(stat -c %s "$OUT")
sha=$(sha256sum "$OUT" | awk '{print $1}')

# Parse the public LAS header in pure Python (no extra deps).
read pt_count pt_format hdr_size xmin xmax ymin ymax zmin zmax <<<"$(
python3 - "$OUT" <<'PY'
import struct, sys
with open(sys.argv[1], "rb") as f:
    b = f.read(375)
assert b[0:4] == b"LASF", "input is not LAS/LAZ"
hdr_size      = struct.unpack("<H", b[94:96])[0]
pt_format     = b[104] & 0x3F  # strip LAZ compression bits
n_legacy      = struct.unpack("<I", b[107:111])[0]
n_long        = struct.unpack("<Q", b[247:255])[0] if len(b) >= 255 else 0
pt_count      = n_long if n_long else n_legacy
xmax, xmin    = struct.unpack("<dd", b[179:195])
ymax, ymin    = struct.unpack("<dd", b[195:211])
zmax, zmin    = struct.unpack("<dd", b[211:227])
print(pt_count, pt_format, hdr_size, xmin, xmax, ymin, ymax, zmin, zmax)
PY
)"

mkdir -p "$(dirname "$META")"
{
  echo "# Run metadata"
  echo "ts                    $(date -Iseconds)"
  echo "host                  $(hostname)"
  echo "uname                 $(uname -srm)"
  echo "cpu_model             $(awk -F': ' '/model name/{print $2; exit}' /proc/cpuinfo)"
  echo "cpu_threads           $(nproc)"
  echo "ram_gb                $(awk '/MemTotal/{printf "%.1f", $2/1024/1024}' /proc/meminfo)"
  echo "lasR_git_sha          $(git -C ../.. rev-parse HEAD)"
  echo "lasR_git_branch       $(git -C ../.. rev-parse --abbrev-ref HEAD)"
  echo "dataset_url           $URL"
  echo "dataset_path          $OUT"
  echo "dataset_bytes         $bytes"
  echo "dataset_sha256        $sha"
  echo "dataset_point_count   $pt_count"
  echo "dataset_point_format  $pt_format"
  echo "dataset_xmin          $xmin"
  echo "dataset_xmax          $xmax"
  echo "dataset_ymin          $ymin"
  echo "dataset_ymax          $ymax"
  echo "dataset_zmin          $zmin"
  echo "dataset_zmax          $zmax"
} > "$META"

echo "[fetch] OK: $OUT ($bytes bytes, $pt_count points)"
echo "[fetch] metadata at $META"

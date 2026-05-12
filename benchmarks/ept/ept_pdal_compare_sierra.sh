#!/usr/bin/env bash
# Compare lasR EPT acquisition vs PDAL on the same remote endpoint and AOI.
# Variant of ept_pdal_compare.sh targeting a denser site.
#
# Endpoint:  USGS 3DEP CA_SierraNevada_12_B22 (native EPSG:3857, ~80.5B points)
# AOI:       2,049 acres (2,880 m × 2,880 m), same as the lasR Sierra benchmark
#            (sized for ~58M points to match AZ benchmark workload)
# Modes:
#   A. PDAL single-process with `readers.ept` `requests=N` (N in 1, 2, 4, 8, 16)
#   B. PDAL 16 concurrent processes, AOI split into a 4×4 grid (requests=1 each)

set -u
export PROJ_DATA=$HOME/miniforge3/share/proj
PDAL=$HOME/miniforge3/bin/pdal
URL="https://s3-us-west-2.amazonaws.com/usgs-lidar-public/CA_SierraNevada_12_B22/ept.json"

# AOI must match exactly what lasR benchmarked (Sierra variant).
# cx=-13391922  cy=4532635  h=1440.0
XMIN=-13393362.0;  YMIN=4531195.0
XMAX=-13390482.0;  YMAX=4534075.0

WORK=$(mktemp -d /tmp/pdal_bench_sierra.XXXXXX)
trap "rm -rf '$WORK'" EXIT

mk_pipeline() {
  # $1=xmin $2=xmax $3=ymin $4=ymax $5=requests $6=outfile
  cat > "$6" <<EOF
{
  "pipeline": [
    {
      "type": "readers.ept",
      "filename": "$URL",
      "bounds": "([$1, $2], [$3, $4])",
      "requests": $5
    },
    {
      "type": "filters.stats"
    }
  ]
}
EOF
}

run_one() {
  local label="$1"; shift
  local pipeline="$1"; shift
  echo "  starting: $label"
  local t0 t1
  t0=$(date +%s.%N)
  "$PDAL" pipeline "$pipeline" --metadata "$pipeline.meta.json" \
    >/dev/null 2>"$pipeline.err"
  local rc=$?
  t1=$(date +%s.%N)
  local dt
  dt=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')
  if [ $rc -ne 0 ]; then
    echo "  FAILED: $label  rc=$rc  ($(tail -2 "$pipeline.err" | tr '\n' ' '))"
    return 1
  fi
  local npts
  npts=$(python3 - "$pipeline.meta.json" <<'PY'
import json, sys
m = json.load(open(sys.argv[1]))
st = m.get("stages",{}).get("filters.stats",{}).get("statistic",[])
print(st[0]["count"] if st else 0)
PY
)
  echo "  done:  $label  npts=$npts  wall=${dt}s"
  echo "$label	$dt	$npts" >> "$WORK/results.tsv"
}

echo "Endpoint: $URL"
echo "AOI [xmin ymin xmax ymax]: $XMIN $YMIN $XMAX $YMAX"
echo "Area: $(python3 -c "print(round((${XMAX}-${XMIN})*(${YMAX}-${YMIN})/4046.8564224))")  acres"
echo

# --- A. Single-process, varying requests=N ----
echo "[A] Single PDAL process, varying requests=N in readers.ept"
for R in 1 2 4 8 16; do
  PL="$WORK/single_r${R}.json"
  mk_pipeline "$XMIN" "$XMAX" "$YMIN" "$YMAX" "$R" "$PL"
  run_one "single_requests=${R}" "$PL"
done

echo
# --- B. 16 concurrent processes, 4x4 AOI grid, requests=1 each ----
echo "[B] 16 concurrent PDAL processes, 4×4 AOI grid"
ROWS=4; COLS=4
DX=$(awk -v a="$XMAX" -v b="$XMIN" -v n="$COLS" 'BEGIN{printf "%.6f", (a-b)/n}')
DY=$(awk -v a="$YMAX" -v b="$YMIN" -v n="$ROWS" 'BEGIN{printf "%.6f", (a-b)/n}')

for r in $(seq 0 $((ROWS-1))); do
  for c in $(seq 0 $((COLS-1))); do
    x0=$(awk -v xmin="$XMIN" -v dx="$DX" -v c="$c" 'BEGIN{printf "%.6f", xmin + c*dx}')
    x1=$(awk -v xmin="$XMIN" -v dx="$DX" -v c="$c" 'BEGIN{printf "%.6f", xmin + (c+1)*dx}')
    y0=$(awk -v ymin="$YMIN" -v dy="$DY" -v r="$r" 'BEGIN{printf "%.6f", ymin + r*dy}')
    y1=$(awk -v ymin="$YMIN" -v dy="$DY" -v r="$r" 'BEGIN{printf "%.6f", ymin + (r+1)*dy}')
    mk_pipeline "$x0" "$x1" "$y0" "$y1" 1 "$WORK/grid_${r}_${c}.json"
  done
done

echo "  starting: parallel_16proc"
t0=$(date +%s.%N)
ls "$WORK"/grid_*.json | xargs -n1 -P16 -I{} bash -c \
  '"'"$PDAL"'" pipeline "{}" --metadata "{}.meta.json" >/dev/null 2>"{}.err"; echo "{} done"' \
  > "$WORK/parallel.log" 2>&1
t1=$(date +%s.%N)
DT_PAR=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')
NTOT=$(python3 - "$WORK" <<'PY'
import json, glob, sys, os
base = sys.argv[1]
total = 0
for m in sorted(glob.glob(os.path.join(base, "grid_*.json.meta.json"))):
    try:
        d = json.load(open(m))
        st = d["stages"]["filters.stats"]["statistic"]
        total += st[0]["count"] if st else 0
    except Exception as e:
        print(f"  WARN: {m}: {e}", file=sys.stderr)
print(total)
PY
)
echo "  done:  parallel_16proc  npts(sum)=$NTOT  wall=${DT_PAR}s"
echo "parallel_16proc	$DT_PAR	$NTOT" >> "$WORK/results.tsv"

echo
echo "[summary]"
python3 - "$WORK/results.tsv" <<'PY'
import sys
rows = []
for line in open(sys.argv[1]):
    label, dt, npts = line.rstrip("\n").split("\t")
    rows.append((label, float(dt), int(npts)))
base = next(t for l,t,_ in rows if l == "single_requests=1")
print(f"  {'mode':28s}  {'wall(s)':>9s}  {'speedup':>9s}  {'npts':>12s}")
for l,t,n in rows:
    print(f"  {l:28s}  {t:9.2f}  {base/t:8.2f}x  {n:12d}")
ns = set(n for _,_,n in rows)
if len(ns) == 1:
    print(f"\n  All modes returned npts = {ns.pop()} → correctness OK")
else:
    print(f"\n  WARNING: npts differ across modes: {ns}")
PY

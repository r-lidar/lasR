#!/usr/bin/env bash
# Runs read scenarios via local httpuv server. Idempotent.
set -euo pipefail
cd "$(dirname "$0")"

PORT=${BENCH_PORT:-38080}
RID_FILE=results/.run_id
LOG_CSV=results/http_log.csv
READ_CSV=results/read.csv

mkdir -p results
: > "$RID_FILE"

preflight_target=$(ls results/outputs/*.copc.laz 2>/dev/null | head -1)
test -n "$preflight_target" || { echo "[run_reads] no COPCs in results/outputs/" >&2; exit 1; }

Rscript range_logging_app.R results/outputs "$PORT" "$LOG_CSV" "$RID_FILE" &
SRV=$!
trap 'kill "$SRV" 2>/dev/null || true; wait "$SRV" 2>/dev/null || true' EXIT
sleep 1

name=$(basename "$preflight_target")
hdrs=$(curl -sS -o /dev/null -D - -H 'Range: bytes=0-15' "http://127.0.0.1:$PORT/$name")
echo "$hdrs" | grep -qi '^HTTP/.* 206' || { echo "[preflight] no 206" >&2; exit 1; }
echo "$hdrs" | grep -qi '^Content-Range:' || { echo "[preflight] no Content-Range" >&2; exit 1; }
echo "$hdrs" | grep -qi '^Accept-Ranges: bytes' || { echo "[preflight] no Accept-Ranges" >&2; exit 1; }
clen=$(echo "$hdrs" | awk -F': ' 'tolower($1)=="content-length"{gsub(/\r/,"",$2); print $2}')
[[ "$clen" == "16" ]] || { echo "[preflight] expected Content-Length 16, got '$clen'" >&2; exit 1; }

head_hdrs=$(curl -sS -I -H 'Range: bytes=0-15' "http://127.0.0.1:$PORT/$name")
echo "$head_hdrs" | grep -qi '^HTTP/.* 206' || { echo "[preflight] HEAD no 206" >&2; exit 1; }
head_clen=$(echo "$head_hdrs" | awk -F': ' 'tolower($1)=="content-length"{gsub(/\r/,"",$2); print $2}')
[[ "$head_clen" == "16" ]] || { echo "[preflight] HEAD expected Content-Length 16, got '$head_clen'" >&2; exit 1; }
echo "[preflight] OK"

Rscript bench_read.R m2 "$LOG_CSV" "$RID_FILE" "$READ_CSV" "http://127.0.0.1:$PORT"

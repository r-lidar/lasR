#!/usr/bin/env bash
# Runs every writer N times against data/input.laz, captures wall time,
# peak RSS, exit status. Produces write.csv + logs/. Promotes one warm
# output per writer to results/outputs/. Calls inspect_copc.R for each.
set -euo pipefail
shopt -s lastpipe

cd "$(dirname "$0")"
INPUT=data/input.laz
RES=results
LOGS=$RES/logs
OUTS=$RES/outputs
WRITE_CSV=$RES/write.csv
LOD_SUMMARY_CSV=$RES/lod_summary.csv
test -s "$INPUT" || { echo "[run_writers] missing $INPUT — run fetch_dataset.sh first" >&2; exit 1; }
mkdir -p "$LOGS" "$OUTS" "$RES/scratch"
test -s "$RES/run_metadata.txt" || { echo "[run_writers] missing $RES/run_metadata.txt — run fetch_dataset.sh first" >&2; exit 1; }

WARM_REPEATS=${WARM_REPEATS:-3}
DROP_CACHES=${DROP_CACHES:-auto}   # auto|always|never
INPUT_POINTS=$(awk '$1=="dataset_point_count"{print $2; exit}' "$RES/run_metadata.txt")

# Writer matrix: <writer_id>|<wrapper_path>|<extra_argv_prefix>
WRITERS=(
  "lasR-default|writers/write_lasR.R|default"
  "lasR-experimental|writers/write_lasR.R|experimental"
  "pdal|writers/write_pdal.sh|"
  "untwine|writers/write_untwine.sh|"
  "lascopcindex|writers/write_lascopcindex.sh|"
)

if [[ ! -s "$WRITE_CSV" ]]; then
  echo "writer_id,run_kind,run_idx,wall_s,peak_rss_mb,exit_status,output_bytes,output_path,stdout_log,stderr_log,time_log,command_line,host,ts" > "$WRITE_CSV"
fi
if [[ ! -s "$LOD_SUMMARY_CSV" ]]; then
  echo "writer_id,max_depth,n_nodes,n_chunks,total_points,host,ts" > "$LOD_SUMMARY_CSV"
fi

validate_copc() {
  local writer_id=$1 path=$2 stem=$3
  local pdal_json="$LOGS/${stem}.pdal_info.json"
  local npoints

  if ! pdal info --summary "$path" > "$pdal_json" 2>"$LOGS/${stem}.pdal_info.err"; then
    echo "[validate] $writer_id: pdal info failed for $path" >&2
    return 1
  fi

  if ! npoints=$(Rscript -e '
    suppressPackageStartupMessages(library(jsonlite))
    x <- fromJSON(paste(readLines(commandArgs(TRUE)[1], warn = FALSE), collapse = "\n"),
                  simplifyVector = FALSE)
    find_count <- function(v) {
      if (is.list(v)) {
        if (!is.null(v$num_points)) return(v$num_points)
        if (!is.null(v$points)) return(v$points)
        for (elt in v) {
          z <- find_count(elt)
          if (!is.null(z)) return(z)
        }
      }
      NULL
    }
    z <- find_count(x)
    if (is.null(z)) quit(status = 2)
    cat(format(z, scientific = FALSE, trim = TRUE))
  ' "$pdal_json"); then
    echo "[validate] $writer_id: could not extract point count from pdal summary" >&2
    return 1
  fi

  if [[ -n "${INPUT_POINTS:-}" && "$npoints" != "$INPUT_POINTS" ]]; then
    echo "[validate] $writer_id: point-count mismatch, got $npoints expected $INPUT_POINTS" >&2
    return 1
  fi

  if ! grep -Eqi '"reader"[[:space:]]*:[[:space:]]*"readers\.copc"' "$pdal_json"; then
    echo "[validate] $writer_id: pdal summary does not mention COPC" >&2
    return 1
  fi
}

drop_caches() {
  case "$DROP_CACHES" in
    never)  return 0 ;;
    always|auto) ;;
    *) echo "[run_writers] bad DROP_CACHES=$DROP_CACHES" >&2; return 1 ;;
  esac
  if sudo -n true 2>/dev/null; then
    sync && echo 3 | sudo -n tee /proc/sys/vm/drop_caches > /dev/null
  else
    [[ "$DROP_CACHES" == "always" ]] && {
      echo "[run_writers] DROP_CACHES=always but no passwordless sudo" >&2; return 1; }
  fi
}

run_one() {
  local writer_id=$1 wrapper=$2 extra=$3 kind=$4 idx=$5
  local stem="${writer_id}__${kind}__${idx}"
  local out="$RES/scratch/${stem}.copc.laz"
  local log_stdout="$LOGS/${stem}.out"
  local log_stderr="$LOGS/${stem}.err"
  local time_log="$LOGS/${stem}.time"
  rm -f "$out"

  local cmd=(/usr/bin/time -v -o "$time_log" "$wrapper")
  [[ -n "$extra" ]] && cmd+=("$extra")
  cmd+=("$INPUT" "$out")

  local cmd_str="${cmd[*]}"

  local exit_status=0
  set +e
  "${cmd[@]}" >"$log_stdout" 2>"$log_stderr"
  exit_status=$?
  set -e

  # Parse /usr/bin/time -v
  local wall_s peak_kb output_bytes
  wall_s=$(awk -F': ' '/Elapsed \(wall clock\) time/{print $2}' "$time_log" \
             | awk -F: '{ if (NF==3) printf "%.3f",$1*3600+$2*60+$3; \
                          else if (NF==2) printf "%.3f",$1*60+$2; \
                          else print $1 }')
  peak_kb=$(awk -F': ' '/Maximum resident set size/{print $2}' "$time_log")
  if [[ -s "$out" ]]; then output_bytes=$(stat -c %s "$out"); else output_bytes=0; fi
  local peak_mb
  peak_mb=$(awk -v k="${peak_kb:-0}" 'BEGIN{printf "%.1f", k/1024}')

  # CSV-quote command line
  local cmd_csv="\"${cmd_str//\"/\"\"}\""
  printf "%s,%s,%d,%s,%s,%d,%d,%s,%s,%s,%s,%s,%s,%s\n" \
    "$writer_id" "$kind" "$idx" "${wall_s:-NA}" "$peak_mb" \
    "$exit_status" "$output_bytes" "$out" "$log_stdout" "$log_stderr" "$time_log" "$cmd_csv" \
    "$(hostname)" "$(date -Iseconds)" >> "$WRITE_CSV"

  echo "[run_writers] $writer_id $kind#$idx wall=${wall_s}s rss=${peak_mb}MB status=$exit_status"
  if [[ $exit_status -ne 0 ]]; then return $exit_status; fi
}

# write phase per writer: 1 cold + N warm
for entry in "${WRITERS[@]}"; do
  IFS='|' read -r writer_id wrapper extra <<<"$entry"
  echo "=== writer: $writer_id ==="
  drop_caches
  run_one "$writer_id" "$wrapper" "$extra" cold 0 || true

  for i in $(seq 1 "$WARM_REPEATS"); do
    run_one "$writer_id" "$wrapper" "$extra" warm "$i" || true
  done

  # Promote first successful warm output
  promoted=
  for i in $(seq 1 "$WARM_REPEATS"); do
    cand="$RES/scratch/${writer_id}__warm__${i}.copc.laz"
    if [[ -s "$cand" ]] && validate_copc "$writer_id" "$cand" "${writer_id}__warm__${i}"; then
      promoted=$cand
      break
    fi
  done
  if [[ -z "$promoted" ]]; then
    echo "[run_writers] $writer_id: no warm output succeeded — skipping promotion + LOD" >&2
    continue
  fi
  cp "$promoted" "$OUTS/${writer_id}.copc.laz"
  lod_csv="$RES/lod_${writer_id}.csv"
  Rscript inspect_copc.R "$writer_id" "$OUTS/${writer_id}.copc.laz" "$lod_csv"
  lod_summary=$(Rscript -e '
    d <- read.csv(commandArgs(TRUE)[1])
    if (!nrow(d)) stop("empty LOD CSV")
    cat(max(d$depth), nrow(d), nrow(d),
        format(sum(d$point_count), scientific = FALSE, trim = TRUE),
        sep = ",")
  ' "$lod_csv")
  printf "%s,%s,%s,%s\n" "$writer_id" "$lod_summary" "$(hostname)" "$(date -Iseconds)" >> "$LOD_SUMMARY_CSV"
done

echo "[run_writers] done. write.csv has $(wc -l < "$WRITE_CSV") lines (incl. header)."

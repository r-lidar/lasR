#!/usr/bin/env bash
# Usage: write_pdal.sh <input_laz> <output_copc_laz>
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <input> <output>" >&2
  exit 64
fi
in=$1; out=$2

dir=$(dirname "$0")
tmpl="$dir/write_pdal.json.tmpl"
pipeline=$(mktemp --suffix=.json)
trap 'rm -f "$pipeline"' EXIT

# inject paths (use | as sed separator since paths contain /)
sed "s|@INPUT@|$in|; s|@OUTPUT@|$out|" "$tmpl" > "$pipeline"

pdal pipeline "$pipeline"
test -s "$out"
echo "[write_pdal] wrote $out ($(stat -c %s "$out") bytes)"

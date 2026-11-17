#!/usr/bin/env bash
# Usage: write_untwine.sh <input_laz> <output_copc_laz>
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <input> <output>" >&2
  exit 64
fi
in=$1; out=$2

mkdir -p "$(dirname "$out")"
untwine -i "$in" -o "$out"

if [[ ! -f "$out" ]]; then
  echo "[write_untwine] no output produced at $out" >&2
  exit 1
fi

test -s "$out"
echo "[write_untwine] wrote $out ($(stat -c %s "$out") bytes)"

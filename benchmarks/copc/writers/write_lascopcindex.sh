#!/usr/bin/env bash
# Usage: write_lascopcindex.sh <input_laz> <output_copc_laz>
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <input> <output>" >&2
  exit 64
fi
in=$(readlink -f "$1")
out=$(readlink -f "$2")

workdir=$(mktemp -d --suffix=.lascopcindex)
trap 'rm -rf "$workdir"' EXIT

# Run inside workdir so any sidecar files land there, not in repo.
( cd "$workdir" && lascopcindex -i "$in" -o "$out" )

test -s "$out"
echo "[write_lascopcindex] wrote $out ($(stat -c %s "$out") bytes)"

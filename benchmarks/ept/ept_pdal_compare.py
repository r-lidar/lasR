#!/usr/bin/env python3
"""Compare lasR EPT acquisition vs PDAL on the same remote endpoint and AOI.

Endpoint:  USGS 3DEP AZ_BrawleyRillito_TL_2018 (native EPSG:3857, ~2B points)
AOI:       10,000 acres (6,361 m x 6,361 m), same as the lasR benchmark
Mode:      Single PDAL process, readers.ept `requests=N` (N in 1, 2, 4, 8, 16),
           writing the AOI out as a LAZ file each run.

Live progress is printed per run.
"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

URL = "https://s3-us-west-2.amazonaws.com/usgs-lidar-public/AZ_BrawleyRillito_TL_2018/ept.json"

# AOI must match exactly what lasR benchmarked.
XMIN, YMIN = -12416511.5, 3755441.5
XMAX, YMAX = -12410150.5, 3761802.5


def make_pipeline(xmin, xmax, ymin, ymax, requests, laz_path, pipeline_path):
    # readers.ept uses bounds as "([xmin, xmax], [ymin, ymax])" and the
    # parallel-fetch knob is `requests`, not `threads`.
    pipeline = {
        "pipeline": [
            {
                "type": "readers.ept",
                "filename": URL,
                "bounds": f"([{xmin}, {xmax}], [{ymin}, {ymax}])",
                "requests": requests,
            },
            {"type": "filters.stats"},
            {
                "type": "writers.las",
                "filename": laz_path,
                "compression": "laszip",
            },
        ]
    }
    with open(pipeline_path, "w") as f:
        json.dump(pipeline, f)


def read_npts(meta_path):
    try:
        with open(meta_path) as f:
            meta = json.load(f)
        stats = meta.get("stages", {}).get("filters.stats", {}).get("statistic", [])
        return stats[0]["count"] if stats else 0
    except Exception as e:
        print(f"  WARN: {meta_path}: {e}", file=sys.stderr)
        return 0


def run_pipeline(pipeline_path, laz_path):
    """Execute one PDAL pipeline. Returns (rc, wall_seconds, npts, laz_MiB, err_tail)."""
    meta_path = f"{pipeline_path}.meta.json"
    err_path = f"{pipeline_path}.err"
    t0 = time.monotonic()
    with open(err_path, "w") as err:
        rc = subprocess.call(
            ["pdal", "pipeline", pipeline_path, "--metadata", meta_path],
            stdout=subprocess.DEVNULL,
            stderr=err,
        )
    dt = time.monotonic() - t0
    if rc != 0:
        with open(err_path) as f:
            tail = " ".join(line.strip() for line in f.readlines()[-2:])
        return rc, dt, 0, 0.0, tail
    laz = Path(laz_path)
    size_mib = laz.stat().st_size / (1024 * 1024) if laz.exists() else 0.0
    return rc, dt, read_npts(meta_path), size_mib, ""


def main():
    os.environ.setdefault("PROJ_DATA", os.path.expanduser("~/miniforge3/share/proj"))

    area_acres = round((XMAX - XMIN) * (YMAX - YMIN) / 4046.8564224)
    print(f"Endpoint: {URL}")
    print(f"AOI [xmin ymin xmax ymax]: {XMIN} {YMIN} {XMAX} {YMAX}")
    print(f"Area: {area_acres}  acres")
    print()

    work = Path(tempfile.mkdtemp(prefix="pdal_bench."))
    try:
        results = []
        print("Single PDAL process, varying requests=N (writing LAZ output)")
        for r in (1, 2, 4, 8, 16):
            pl = work / f"single_r{r}.json"
            laz = work / f"single_r{r}.laz"
            label = f"single_requests={r}"
            make_pipeline(XMIN, XMAX, YMIN, YMAX, r, str(laz), str(pl))
            print(f"  starting: {label}", flush=True)
            rc, dt, npts, size_mib, tail = run_pipeline(str(pl), str(laz))
            if rc != 0:
                print(f"  FAILED: {label}  rc={rc}  ({tail})", flush=True)
                continue
            print(
                f"  done:  {label}  npts={npts}  wall={dt:.2f}s  laz={size_mib:.1f} MiB",
                flush=True,
            )
            results.append((label, dt, npts, size_mib))

        if not results:
            sys.exit("no successful runs")

        print()
        print("[summary]")
        base = next(t for l, t, _, _ in results if l == "single_requests=1")
        print(
            f"  {'mode':24s}  {'wall(s)':>9s}  {'speedup':>9s}  "
            f"{'npts':>12s}  {'laz(MiB)':>9s}"
        )
        for l, t, n, s in results:
            print(f"  {l:24s}  {t:9.2f}  {base/t:8.2f}x  {n:12d}  {s:9.1f}")
        unique = {n for _, _, n, _ in results}
        if len(unique) == 1:
            print(f"\n  All runs returned npts = {unique.pop()} -> correctness OK")
        else:
            print(f"\n  WARNING: npts differ across runs: {unique}")
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()

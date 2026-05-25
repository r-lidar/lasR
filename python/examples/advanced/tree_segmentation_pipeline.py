#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
import json
import math
import os
import time
import urllib.request
from contextlib import contextmanager
from pathlib import Path

import numpy as np
import pylasr
import rasterio
from osgeo import gdal, ogr, osr

gdal.UseExceptions()
ogr.UseExceptions()
osr.UseExceptions()


def env_path(name: str, default: str | Path) -> Path:
    return Path(os.environ.get(name, str(default)))


_REMOTE_SCHEMES = ("http://", "https://", "s3://", "gs://", "ftp://")


def is_remote(uri: str) -> bool:
    """Return True for URIs that lasR's engine handles remotely (HTTP/S3/etc)."""
    return any(uri.startswith(prefix) for prefix in _REMOTE_SCHEMES)


def env_input(name: str, default: str) -> str:
    """Read the input source path/URL as a raw string so URL schemes are preserved.

    Wrapping a URL in pathlib.Path collapses ``https://x`` to ``https:/x``; using a
    str keeps remote ept.json sources usable.
    """
    return os.environ.get(name, default)


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT_LAS = REPO_ROOT / "benchmarks" / "copc" / "data" / "input.laz"


def env_float(name: str, default: float) -> float:
    raw = os.environ.get(name)
    if raw is None or raw == "":
        return default
    try:
        return float(raw)
    except ValueError as exc:
        raise ValueError(f"Env {name} must be numeric, got {raw!r}") from exc


def env_bbox(name: str) -> tuple[float, float, float, float] | None:
    raw = os.environ.get(name)
    if raw is None or raw.strip() == "":
        return None
    parts = [p.strip() for p in raw.split(",")]
    if len(parts) != 4:
        raise ValueError(
            f"Env {name} must be 'xmin,ymin,xmax,ymax', got {raw!r}"
        )
    try:
        xmin, ymin, xmax, ymax = (float(p) for p in parts)
    except ValueError as exc:
        raise ValueError(f"Env {name} values must be numeric, got {raw!r}") from exc
    if not (xmin < xmax and ymin < ymax):
        raise ValueError(f"Env {name} requires xmin<xmax and ymin<ymax, got {raw!r}")
    return xmin, ymin, xmax, ymax


@dataclass(frozen=True)
class Config:
    input_las: str
    out_copc: Path
    out_tops: Path
    out_crowns: Path
    chm_raster: Path
    tree_raster: Path
    raw_tops: Path
    min_height: float
    window_size: float
    chm_res: float
    max_cr: float
    parallel_mode: str
    workers: int
    max_edge: float
    aoi: tuple[float, float, float, float] | None
    aoi_depth: int
    verbose: bool
    progress: bool
    profile_file: Path | None
    write_copc: bool
    lmx_classes: str

    @classmethod
    def from_env(cls) -> Config:
        scratch_dir = env_path("LASR_TREE_SCRATCH_DIR", "/tmp")
        scratch_prefix = os.environ.get("LASR_TREE_SCRATCH_PREFIX", "tree_segmentation")

        return cls(
            input_las=env_input("LASR_TREE_INPUT", str(DEFAULT_INPUT_LAS)),
            out_copc=env_path("LASR_TREE_OUT_COPC", "/tmp/input_treeid.copc.laz"),
            out_tops=env_path("LASR_TREE_OUT_TOPS", "/tmp/tree_tops.fgb"),
            out_crowns=env_path("LASR_TREE_OUT_CROWNS", "/tmp/tree_crowns.fgb"),
            chm_raster=env_path(
                "LASR_TREE_OUT_CHM_RASTER", scratch_dir / f"{scratch_prefix}_chm.tif"
            ),
            tree_raster=env_path(
                "LASR_TREE_OUT_TREE_RASTER",
                scratch_dir / f"{scratch_prefix}_treeid.tif",
            ),
            raw_tops=env_path(
                "LASR_TREE_OUT_RAW_TOPS",
                scratch_dir / f"{scratch_prefix}_tops_raw.gpkg",
            ),
            min_height=env_float("LASR_TREE_MIN_HEIGHT", 2.0),
            window_size=env_float("LASR_TREE_WINDOW_SIZE", 5.0),
            chm_res=env_float("LASR_TREE_CHM_RES", 1.0),
            max_cr=env_float("LASR_TREE_MAX_CR", 10.0),
            parallel_mode=os.environ.get("LASR_TREE_PARALLEL", "sequential").lower(),
            workers=int(os.environ.get("LASR_TREE_WORKERS", "0")),
            max_edge=env_float("LASR_TREE_MAX_EDGE", 0.0),
            aoi=env_bbox("LASR_TREE_AOI"),
            aoi_depth=int(os.environ.get("LASR_TREE_AOI_DEPTH", "-1")),
            verbose=os.environ.get("LASR_TREE_VERBOSE", "0") not in ("", "0", "false", "False"),
            progress=os.environ.get("LASR_TREE_PROGRESS", "0") not in ("", "0", "false", "False"),
            profile_file=(
                env_path("LASR_TREE_PROFILE_FILE", "")
                if os.environ.get("LASR_TREE_PROFILE_FILE", "")
                else None
            ),
            write_copc=os.environ.get("LASR_TREE_WRITE_COPC", "1") not in ("", "0", "false", "False"),
            # Space-separated ASPRS classes that the local_maximum stage will
            # consider as candidate tree tops. Default "1 3 4 5" = unclassified
            # + low/med/high vegetation; this suppresses spurious tops over
            # water (class 9), noise (7, 18), buildings (6), bridges (17).
            # Set to "" or "0" to disable the class filter entirely (legacy
            # behavior — keeps every HAG-above-threshold point).
            lmx_classes=os.environ.get("LASR_TREE_LMX_CLASSES", "1 3 4 5").strip(),
        )


CFG = Config.from_env()


def _half_cores() -> int:
    return max(1, (os.cpu_count() or 2) // 2)


def _worker_count() -> int:
    return CFG.workers if CFG.workers > 0 else _half_cores()


def _read_mem_available_bytes() -> int | None:
    """Best-effort available RAM in bytes (Linux /proc/meminfo MemAvailable)."""
    try:
        with open("/proc/meminfo") as fh:
            for line in fh:
                if line.startswith("MemAvailable:"):
                    return int(line.split()[1]) * 1024  # kB -> bytes
    except (OSError, ValueError, IndexError):
        pass
    return None


def _fetch_ept_metadata(uri: str) -> dict | None:
    """Load ept.json (remote URL or local path). Returns None on any failure."""
    try:
        if is_remote(uri):
            with urllib.request.urlopen(uri, timeout=30) as resp:
                return json.load(resp)
        with open(uri) as fh:
            return json.load(fh)
    except (OSError, ValueError) as exc:
        print(
            f"[auto-partition] could not read EPT metadata ({exc}); "
            "leaving partition count to engine default",
            flush=True,
        )
        return None


def _auto_ept_partitions(workers: int) -> int | None:
    """Size the EPT partition count so peak memory fits available RAM.

    Peak memory during concurrent-files processing is roughly
    ``workers x points_per_chunk x bytes_per_point``, and
    ``points_per_chunk = aoi_points / partitions``. Solving for the smallest
    safe partition count gives
    ``partitions = aoi_points / (budget / (workers x bytes_per_point))``.

    ``aoi_points`` is estimated from the archive's *average* map-space density
    (ept.json ``points`` / conforming-bounds area) times the AOI area. Real
    AOIs tend to sit in denser-than-average regions (valleys, forest), so the
    estimate is inflated by ``LASR_TREE_DENSITY_HEADROOM`` (default 3.0) for
    safety -- on CA_CarrHirzDeltaFires the local AOI ran ~3x the archive mean.
    Returns None when any input is missing (caller then leaves the engine
    default / user env in place).
    """
    if CFG.aoi is None:
        return None
    meta = _fetch_ept_metadata(CFG.input_las)
    if not meta:
        return None
    try:
        total_points = float(meta["points"])
        cxmin, cymin, _, cxmax, cymax, _ = meta["boundsConforming"]
    except (KeyError, ValueError, TypeError):
        return None
    map_area = (cxmax - cxmin) * (cymax - cymin)
    if map_area <= 0 or total_points <= 0:
        return None
    map_density = total_points / map_area  # points per map-unit^2

    xmin, ymin, xmax, ymax = CFG.aoi
    aoi_area = (xmax - xmin) * (ymax - ymin)
    headroom = env_float("LASR_TREE_DENSITY_HEADROOM", 3.0)
    aoi_points = map_density * aoi_area * headroom

    avail = _read_mem_available_bytes()
    if avail is None:
        return None
    fraction = env_float("LASR_TREE_MEM_FRACTION", 0.7)
    bytes_per_point = env_float("LASR_TREE_BYTES_PER_POINT", 100.0)
    pts_per_slot = (avail * fraction) / (max(1, workers) * bytes_per_point)
    if pts_per_slot <= 0:
        return None

    target = max(workers, min(4096, math.ceil(aoi_points / pts_per_slot)))

    print(
        f"[auto-partition] density~{map_density:.1f} pts/m^2 . "
        f"AOI {aoi_area:.3g} m^2 . est~{aoi_points:.3g} pts (headroom x{headroom:g})",
        flush=True,
    )
    print(
        f"[auto-partition] mem avail {avail / 1e9:.1f} GB x{fraction:g} / "
        f"({workers} workers x{bytes_per_point:g} B/pt) -> "
        f"{pts_per_slot / 1e6:.1f}M pts/slot -> LASR_EPT_PARTITIONS={target}",
        flush=True,
    )
    return target


def _maybe_set_ept_partitions() -> None:
    """Export a memory-sized LASR_EPT_PARTITIONS for concurrent-files EPT runs.

    No-op unless: concurrent-files strategy, a remote ept.json input, an AOI is
    set, and the user has not pinned LASR_EPT_PARTITIONS themselves (env wins).
    """
    if CFG.parallel_mode != "concurrent-files":
        return
    if not (
        is_remote(CFG.input_las)
        and CFG.input_las.rstrip("/").endswith("ept.json")
    ):
        return
    if os.environ.get("LASR_EPT_PARTITIONS"):
        return  # explicit user override wins
    target = _auto_ept_partitions(_worker_count())
    if target is not None:
        os.environ["LASR_EPT_PARTITIONS"] = str(target)


def _apply_parallel_strategy(pipeline) -> None:
    if CFG.parallel_mode == "sequential":
        pipeline.set_sequential_strategy()
    elif CFG.parallel_mode == "concurrent-points":
        pipeline.set_concurrent_points_strategy(_worker_count())
    elif CFG.parallel_mode == "concurrent-files":
        pipeline.set_concurrent_files_strategy(_worker_count())
    else:
        raise ValueError(
            "LASR_TREE_PARALLEL must be sequential|concurrent-points|concurrent-files, "
            f"got {CFG.parallel_mode!r}"
        )


_STAGE_TIMES: list[tuple[str, float]] = []


@contextmanager
def stage_timer(label: str):
    start = time.perf_counter()
    print(f"[stage] {label}: start", flush=True)
    try:
        yield
    finally:
        elapsed = time.perf_counter() - start
        _STAGE_TIMES.append((label, elapsed))
        print(f"[stage] {label}: {elapsed:.2f}s", flush=True)


def remove_outputs(*paths: Path) -> None:
    for path in paths:
        if path.exists():
            path.unlink()


def ensure_parent_dirs(*paths: Path) -> None:
    for path in paths:
        path.parent.mkdir(parents=True, exist_ok=True)


def require_success(result: dict, label: str) -> None:
    if not result.get("success"):
        raise RuntimeError(f"{label} failed: {result.get('message', 'unknown error')}")


def execute_pipeline(pipeline, label: str) -> None:
    if CFG.aoi is not None:
        xmin, ymin, xmax, ymax = CFG.aoi
        pipeline = (
            pylasr.reader_rectangles(
                xmin=[xmin],
                ymin=[ymin],
                xmax=[xmax],
                ymax=[ymax],
                depth=CFG.aoi_depth,
            )
            + pipeline
        )

    pipeline.set_progress(CFG.progress)
    pipeline.set_verbose(CFG.verbose)
    if CFG.profile_file is not None:
        pipeline.set_profile_file(str(CFG.profile_file))
    _apply_parallel_strategy(pipeline)
    _maybe_set_ept_partitions()
    pipeline.set_buffer(0)
    pipeline.set_chunk(0)

    result = pipeline.execute(CFG.input_las)
    require_success(result, label)


def run_pylasr_segmentation() -> None:
    cleanup = [CFG.chm_raster, CFG.tree_raster, CFG.raw_tops]
    if CFG.write_copc:
        cleanup.insert(0, CFG.out_copc)
    remove_outputs(*cleanup)

    tri = pylasr.triangulate(
        max_edge=CFG.max_edge, filter=["Classification %in% 2 9"]
    )
    hag_eb = pylasr.add_attribute("double", "HAG", "Height Above Ground")
    hag = pylasr.transform_with(tri, operation="-", store_in_attribute="HAG")
    chm = pylasr.rasterize(
        res=CFG.chm_res,
        window=CFG.chm_res,
        operators=["HAG_max"],
        ofile=str(CFG.chm_raster),
    )
    lmx_filters = [f"HAG > {CFG.min_height:g}"]
    if CFG.lmx_classes and CFG.lmx_classes != "0":
        lmx_filters.append(f"Classification %in% {CFG.lmx_classes}")
    lmx = pylasr.local_maximum(
        ws=CFG.window_size,
        min_height=CFG.min_height,
        filter=lmx_filters,
        ofile=str(CFG.raw_tops),
        use_attribute="HAG",
        record_attributes=True,
    )
    tree = pylasr.region_growing(
        chm,
        lmx,
        th_tree=CFG.min_height,
        th_seed=0.45,
        th_cr=0.55,
        max_cr=CFG.max_cr,
        ofile=str(CFG.tree_raster),
    )

    pipeline = tri + hag_eb + hag + chm + lmx + tree
    if CFG.write_copc:
        # Assign per-point treeID directly from the region_growing raster using
        # the '=' operator (nearest-cell sampling preserves the integer label),
        # then write a labeled COPC. Drop HAG before the writer so the output
        # schema carries only treeID.
        tid_eb = pylasr.add_attribute("int", "treeID", "tree segmentation ID")
        tid_set = pylasr.transform_with(
            tree, operation="=", store_in_attribute="treeID", bilinear=False
        )
        drop_hag = pylasr.remove_attribute("HAG")
        writer = pylasr.write_copc(str(CFG.out_copc))
        pipeline = pipeline + tid_eb + tid_set + drop_hag + writer

    execute_pipeline(pipeline, "pylasr segmentation")


class RasterioSampler:
    """Vectorized raster sampler with terra-like edge handling.

    A point sitting exactly on the right or bottom raster boundary is sampled
    from the last cell. For bilinear sampling, NaN neighbours fall back to
    nearest-cell sampling instead of returning NaN.
    """

    EDGE_EPS = 1e-9  # tolerance for "essentially on boundary"

    def __init__(self, path: Path):
        with rasterio.open(path) as dataset:
            self.projection = dataset.crs.to_wkt() if dataset.crs else ""
            self.array = dataset.read(1, masked=True).astype("float64").filled(np.nan)
            self.inverse_transform = ~dataset.transform
            nodata = dataset.nodatavals[0]
            self._right = dataset.bounds.right
            self._bottom = dataset.bounds.bottom
            self._res_x, self._res_y = dataset.res

        self.array[~np.isfinite(self.array)] = np.nan
        self._nodata = float(nodata) if nodata is not None else None

    def _edge_clamp(
        self, x: np.ndarray, y: np.ndarray
    ) -> tuple[np.ndarray, np.ndarray]:
        x = np.asarray(x, dtype="float64").copy()
        y = np.asarray(y, dtype="float64").copy()
        # If the point is on (or barely past) the right/bottom boundary within
        # float tolerance, nudge it half a pixel inside so nearest-cell sampling uses
        # the rightmost/bottommost cell instead of returning nodata.
        right_mask = (x >= self._right - self.EDGE_EPS) & (
            x <= self._right + self.EDGE_EPS
        )
        bottom_mask = (y >= self._bottom - self.EDGE_EPS) & (
            y <= self._bottom + self.EDGE_EPS
        )
        x[right_mask] = self._right - 0.5 * self._res_x
        y[bottom_mask] = self._bottom + 0.5 * self._res_y
        return x, y

    def sample_simple_many(self, x: np.ndarray, y: np.ndarray) -> np.ndarray:
        xs = np.asarray(x, dtype="float64")
        ys = np.asarray(y, dtype="float64")
        xs_adj, ys_adj = self._edge_clamp(xs, ys)

        px = (
            self.inverse_transform.a * xs_adj
            + self.inverse_transform.b * ys_adj
            + self.inverse_transform.c
        )
        py = (
            self.inverse_transform.d * xs_adj
            + self.inverse_transform.e * ys_adj
            + self.inverse_transform.f
        )
        cols = np.floor(px).astype(np.int64)
        rows = np.floor(py).astype(np.int64)

        out = np.full(xs_adj.shape, np.nan, dtype="float64")
        valid = (
            (rows >= 0)
            & (cols >= 0)
            & (rows < self.array.shape[0])
            & (cols < self.array.shape[1])
        )
        out[valid] = self.array[rows[valid], cols[valid]]
        if self._nodata is not None:
            out[out == self._nodata] = np.nan
        out[~np.isfinite(out)] = np.nan
        return out

    def sample_simple(self, x: float, y: float) -> float:
        return float(self.sample_simple_many(np.array([x]), np.array([y]))[0])

    def sample_bilinear(self, x: float, y: float) -> float:
        x_adj, y_adj = self._edge_clamp(np.array([x]), np.array([y]))
        x = float(x_adj[0])
        y = float(y_adj[0])
        px, py = self.inverse_transform * (x, y)
        colf = px - 0.5
        rowf = py - 0.5
        col0 = int(math.floor(colf))
        row0 = int(math.floor(rowf))
        dx = colf - col0
        dy = rowf - row0

        h, w = self.array.shape
        if row0 < 0 or col0 < 0 or row0 + 1 >= h or col0 + 1 >= w:
            return self.sample_simple(x, y)

        values = np.array(
            [
                self.array[row0, col0],
                self.array[row0, col0 + 1],
                self.array[row0 + 1, col0],
                self.array[row0 + 1, col0 + 1],
            ],
            dtype=float,
        )
        # Match terra::extract: any NaN neighbour -> degrade to nearest-cell sampling
        if not np.all(np.isfinite(values)):
            return self.sample_simple(x, y)

        top = values[0] * (1.0 - dx) + values[1] * dx
        bottom = values[2] * (1.0 - dx) + values[3] * dx
        return float(top * (1.0 - dy) + bottom * dy)


def create_flatgeobuf_layer(
    path: Path,
    name: str,
    geometry_type: int,
    projection: str,
    fields: list[tuple[str, int]],
) -> tuple[ogr.DataSource, ogr.Layer]:
    remove_outputs(path)
    driver = ogr.GetDriverByName("FlatGeobuf")
    datasource = driver.CreateDataSource(str(path))
    if datasource is None:
        raise RuntimeError(f"Cannot create {path}")

    srs = None
    if projection:
        srs = osr.SpatialReference()
        srs.ImportFromWkt(projection)

    layer = datasource.CreateLayer(name, srs=srs, geom_type=geometry_type)
    if layer is None:
        raise RuntimeError(f"Cannot create layer in {path}")

    for field_name, field_type in fields:
        field_def = ogr.FieldDefn(field_name, field_type)
        if layer.CreateField(field_def) != 0:
            raise RuntimeError(f"Cannot create field {field_name} in {path}")

    return datasource, layer


def read_tops_and_write_outputs() -> tuple[dict[int, dict[str, float]], int]:
    trees = RasterioSampler(CFG.tree_raster)
    projection = trees.projection

    source = ogr.Open(str(CFG.raw_tops))
    if source is None:
        raise RuntimeError(f"Cannot open local maxima vector: {CFG.raw_tops}")
    source_layer = source.GetLayer(0)

    tops_ds, tops_layer = create_flatgeobuf_layer(
        CFG.out_tops,
        "tree_tops",
        ogr.wkbPoint,
        projection,
        [
            ("treeID", ogr.OFTInteger),
            ("Z", ogr.OFTReal),
            ("ground", ogr.OFTReal),
            ("H", ogr.OFTReal),
        ],
    )

    best_apex: dict[int, dict[str, float]] = {}
    top_count = 0
    for feature in source_layer:
        geometry = feature.GetGeometryRef()
        if geometry is None:
            continue

        x = geometry.GetX()
        y = geometry.GetY()
        z = geometry.GetZ()
        tree_id_value = trees.sample_simple(x, y)
        if math.isnan(tree_id_value):
            continue

        tree_id = int(tree_id_value)
        if tree_id <= 0:
            continue

        # local_maximum recorded HAG on each top via use_attribute="HAG" +
        # record_attributes=True; height-above-ground comes straight from the
        # feature, no DTM sampler needed. The lasR-side filter already ensures
        # HAG > min_height, so no further guard is necessary.
        height = float(feature.GetField("HAG"))
        ground = z - height

        output_feature = ogr.Feature(tops_layer.GetLayerDefn())
        output_feature.SetField("treeID", tree_id)
        output_feature.SetField("Z", float(z))
        output_feature.SetField("ground", float(ground))
        output_feature.SetField("H", float(height))

        output_geometry = ogr.Geometry(ogr.wkbPoint)
        output_geometry.AddPoint_2D(x, y)
        output_feature.SetGeometry(output_geometry)
        if tops_layer.CreateFeature(output_feature) != 0:
            raise RuntimeError(f"Cannot write feature to {CFG.out_tops}")
        top_count += 1

        current = best_apex.get(tree_id)
        if (
            current is None
            or height > current["H"]
            or (height == current["H"] and z > current["Z"])
        ):
            best_apex[tree_id] = {"H": float(height), "Z": float(z)}

    tops_ds = None
    source = None
    return best_apex, top_count


def polygonize_tree_raster(best_apex: dict[int, dict[str, float]]) -> int:
    tree_ds = gdal.Open(str(CFG.tree_raster))
    if tree_ds is None:
        raise RuntimeError(f"Cannot open tree raster: {CFG.tree_raster}")

    projection = tree_ds.GetProjection()
    memory_driver = ogr.GetDriverByName("Memory")
    memory_ds = memory_driver.CreateDataSource("tree_polygons")
    memory_layer = memory_ds.CreateLayer("tree_polygons", geom_type=ogr.wkbPolygon)
    memory_layer.CreateField(ogr.FieldDefn("treeID", ogr.OFTInteger))

    band = tree_ds.GetRasterBand(1)
    gdal.Polygonize(band, None, memory_layer, 0, [], callback=None)

    polygon_by_id: dict[int, ogr.Geometry] = {}
    for feature in memory_layer:
        tree_id = int(feature.GetField("treeID"))
        if tree_id <= 0 or tree_id not in best_apex:
            continue

        geometry = feature.GetGeometryRef()
        if geometry is None or geometry.IsEmpty():
            continue

        geometry = geometry.Clone()
        existing = polygon_by_id.get(tree_id)
        if existing is None:
            polygon_by_id[tree_id] = geometry
        else:
            polygon_by_id[tree_id] = existing.Union(geometry)

    crowns_ds, crowns_layer = create_flatgeobuf_layer(
        CFG.out_crowns,
        "tree_crowns",
        ogr.wkbUnknown,
        projection,
        [
            ("treeID", ogr.OFTInteger),
            ("H", ogr.OFTReal),
            ("Z", ogr.OFTReal),
        ],
    )
    crown_count = 0
    for tree_id in sorted(polygon_by_id):
        apex = best_apex[tree_id]
        feature = ogr.Feature(crowns_layer.GetLayerDefn())
        feature.SetField("treeID", tree_id)
        feature.SetField("H", apex["H"])
        feature.SetField("Z", apex["Z"])
        feature.SetGeometry(polygon_by_id[tree_id])
        if crowns_layer.CreateFeature(feature) != 0:
            raise RuntimeError(f"Cannot write feature to {CFG.out_crowns}")
        crown_count += 1

    crowns_ds = None
    memory_ds = None
    tree_ds = None
    return crown_count


def main() -> None:
    if not is_remote(CFG.input_las) and not Path(CFG.input_las).exists():
        raise FileNotFoundError(CFG.input_las)

    ensure_parent_dirs(
        CFG.out_copc,
        CFG.out_tops,
        CFG.out_crowns,
        CFG.chm_raster,
        CFG.tree_raster,
        CFG.raw_tops,
    )

    with stage_timer("lasR pipeline (copc+tri+hag+chm+lmx+region_growing)"):
        run_pylasr_segmentation()
    with stage_timer("read tops + write tree_tops.fgb"):
        best_apex, top_count = read_tops_and_write_outputs()
    with stage_timer("polygonize + write tree_crowns.fgb"):
        crown_count = polygonize_tree_raster(best_apex)

    print(f"Tops: {top_count}")
    print(f"Crowns: {crown_count}")
    print(f"Wrote: {CFG.out_copc}")
    print(f"Wrote: {CFG.out_tops}")
    print(f"Wrote: {CFG.out_crowns}")

    total = sum(t for _, t in _STAGE_TIMES)
    print("\n=== Stage timings (python) ===")
    for label, secs in _STAGE_TIMES:
        share = (secs / total * 100.0) if total > 0 else 0.0
        print(f"  {secs:7.2f}s  ({share:5.1f}%)  {label}")
    print(f"  {total:7.2f}s  (100.0%)  TOTAL")

    if CFG.profile_file is not None and CFG.profile_file.exists():
        print_lasr_profile(CFG.profile_file)


def print_lasr_profile(path: Path) -> None:
    # The lasR Profiler writes CSV "name, start, end, thread". Each row is
    # one (stage, chunk, thread) sample. We aggregate by stage name and
    # report both wall (max-end - min-start across all rows of that stage)
    # and CPU (sum of end-start) so parallel slack is visible.
    rows: list[tuple[str, float, float, int]] = []
    with path.open() as fh:
        header = fh.readline()  # discard
        for line in fh:
            parts = [p.strip() for p in line.rstrip("\n").split(",")]
            if len(parts) < 4:
                continue
            try:
                rows.append((parts[0], float(parts[1]), float(parts[2]), int(parts[3])))
            except ValueError:
                continue
    if not rows:
        return

    by_stage: dict[str, list[tuple[float, float, int]]] = {}
    order: list[str] = []
    for name, s, e, t in rows:
        if name not in by_stage:
            by_stage[name] = []
            order.append(name)
        by_stage[name].append((s, e, t))

    pipeline_wall = max(e for _, _, e, _ in rows) - min(s for _, s, _, _ in rows)
    print(f"\n=== lasR per-stage profile (from {path}) ===")
    print(f"{'stage':<22} {'wall(s)':>9} {'cpu(s)':>9} {'%wall':>7} {'calls':>6}")
    for name in order:
        entries = by_stage[name]
        wall = max(e for _, e, _ in entries) - min(s for s, _, _ in entries)
        cpu = sum(e - s for s, e, _ in entries)
        share = (wall / pipeline_wall * 100.0) if pipeline_wall > 0 else 0.0
        print(f"{name:<22} {wall:>9.2f} {cpu:>9.2f} {share:>7.1f} {len(entries):>6}")
    print(f"{'TOTAL(pipeline wall)':<22} {pipeline_wall:>9.2f}")


if __name__ == "__main__":
    main()

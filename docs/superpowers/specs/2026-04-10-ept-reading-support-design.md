# EPT (Entwine Point Tile) Reading Support

## Context

Many large public LiDAR datasets (USGS 3DEP, Entwine public data, etc.) are published in EPT format and have not been converted to COPC. lasR recently added remote COPC/LAS reading (PR #278) via GDAL's virtual filesystem, but cannot access EPT datasets. This design adds read-only EPT support so users can access these existing datasets through the same `reader()` + `exec()` API.

## Requirements

- **Read-only** — no EPT writing
- **Both local and remote** — EPT directories on disk and HTTP endpoints
- **LAZ tiles only** — reject binary/zstandard EPT datasets with a clear error
- **Auto-detection** — user passes `ept.json` path/URL to `reader()`, lasR detects and handles it
- **Spatial queries** — `reader_circles()`, `reader_rectangles()` work with EPT via octree
- **Depth control** — new `ept_depth` parameter (separate from `copc_depth`)
- **Transparent pipeline** — downstream stages work unchanged

## Architecture

### Data Flow

```
User: exec(reader(ept_depth = 3), on = "https://server/ept.json")
  → FileCollection::parse_path() → EPTFILE
  → FileCollection::add_ept_endpoint()
    → Fetch & parse ept.json (bounds, SRS, schema, dataType)
    → Create synthetic Header with EPT metadata
    → Register as single "file" in collection
  → LASReptreader stage (new)
    → EPTio::query(bbox, depth_limit)
      → Traverse ept-hierarchy/ JSON → find intersecting nodes
      → Queue tile EPTkeys, sorted spatially
    → EPTio::read_point()
      → Open ept-data/D-X-Y-Z.laz via LASreaderLAS
      → Yield points; advance to next tile when exhausted
  → Downstream pipeline stages (unchanged)
```

### Key Design Decision

EPT is treated as a **single entry** in FileCollection (like a COPC file), not as multiple files. The octree navigation and tile fetching happen inside EPTio, transparent to the rest of the pipeline.

## New Components

### 1. EPTio (src/LASRreaders/EPTio.h, EPTio.cpp)

New `Fileio` subclass. Responsibilities:

**`open(path)`** — Fetch and parse `ept.json`:
- Local: `std::ifstream` + nlohmann/json
- Remote: `VSIFOpenL` + `VSIFReadL` + nlohmann/json
- Extract: bounds (cube + conforming), SRS (WKT or EPSG), schema, dataType, span
- Validate: reject if `dataType != "laszip"`
- Store `base_url` (path without trailing `ept.json`)

**`populate_header(Header*)`** — Build lasR Header from EPT metadata:
- Set bounds from `boundsConforming`
- Set CRS from `srs.wkt` or `srs.authority` + `srs.horizontal`
- Determine LAS point data format from schema entries (GPS time? RGB? NIR?)
- Map extra bytes from EPT schema entries not in standard LAS
- Set signature to `"EPTF"`

**`query(main_files, neighbour_files, xmin, ymin, xmax, ymax, buffer, circle, filters)`** — Spatial query:
- Fetch `ept-hierarchy/0-0-0-0.json` (root hierarchy)
- Recursive traversal: for each node, compute 3D bbox from EPT cube bounds + EPTkey
- If bbox intersects query region AND depth <= depth_limit, add to tile queue
- If hierarchy value is `-1`, fetch sub-hierarchy JSON and recurse into it
- Sort tile queue spatially using `spatial_order()` from lascopc.hpp
- Compute total point count from hierarchy values for progress reporting

**`read_point(Point*)`** — Iterate tiles:
- If no tile currently open, pop next EPTkey from queue
- Construct path: `{base_url}/ept-data/{D}-{X}-{Y}-{Z}.laz`
- Open via `LASreaderLAS` (works for local and remote via existing ByteStreamInGDAL)
- Read point, map to lasR Point schema
- When tile exhausted, close and advance to next
- Return `false` when all tiles exhausted

**`set_ept_depth(int depth)`** — Set octree depth limit (-1 = all levels)

**Internal state:**
- `std::string base_url` — EPT root directory/URL
- `nlohmann::json metadata` — Parsed ept.json
- `std::vector<EPTkey> tile_queue` — Tiles to read
- `int current_tile_index` — Position in queue
- `LASreader* current_tile` — Active reader for current tile
- `int depth_limit` — Max depth (-1 = unlimited)
- `int64_t total_points` — Sum from hierarchy for progress
- `int64_t points_read` — Running count

**Not implemented (read-only):**
- `create()` — throws
- `write_point()` — throws

### 2. LASReptreader Stage (src/LASRstages/readept.h, readept.cpp)

Mirrors `LASRlasreader` structure. Uses `EPTio` instead of `LASio`.

```cpp
class LASReptreader : public Stage {
public:
  LASReptreader();
  ~LASReptreader();
  bool process(Header*& header) override;   // populate from EPT metadata
  bool process(Point*& point) override;      // streaming read
  bool process(PointCloud*& las) override;   // batch read
  bool set_chunk(Chunk& chunk) override;     // set up spatial query
  bool need_points() const override { return false; }
  bool is_streamable() const override { return true; }
  std::string get_name() const override { return "reader_ept"; }
  void clear(bool) override;
  LASReptreader* clone() const override;

private:
  Header* header;
  bool streaming;
  EPTio* eptio;
};
```

### 3. FileCollection Changes (src/LASRcore/FileCollection.h, .cpp)

- Add `EPTFILE` to `PathType` enum
- Add `add_ept_endpoint(std::string path)` method:
  - Create temporary EPTio, call `open()` and `populate_header()`
  - Register Header in collection with signature `"EPTF"`
  - Store path (ept.json URL/path) in files vector
- Update `parse_path()`:
  - For local files: detect `.json` extension. If filename is exactly `ept.json`, return EPTFILE. Other `.json` files are not treated as EPT (VPC files use `.vpc` extension, so no conflict).
  - For remote paths: check before the existing REMOTEFILE detection. If URL path component ends with `ept.json`, return EPTFILE. Otherwise fall through to existing REMOTEFILE handling.
- Update `read()`: route `EPTFILE` to `add_ept_endpoint()`
- Update `get_format()`: handle `"EPTF"` signature → return `EPTFILE`

### 4. R/Python API Changes

**R** (`R/stages.R`):
- Add `ept_depth` parameter to `reader()`, `reader_coverage()`, `reader_circles()`, `reader_rectangles()`
- Pass through to C++ API

**C++ API** (`src/LASRapi/`):
- Parse `ept_depth` parameter
- Pass to EPTio via `set_ept_depth()`

**Pipeline dispatch**: When FileCollection format is `EPTFILE`, instantiate `LASReptreader` instead of `LASRlasreader`.

## EPT Format Details

### ept.json Structure

```json
{
  "bounds": [xmin, ymin, zmin, xmax, ymax, zmax],
  "boundsConforming": [xmin, ymin, zmin, xmax, ymax, zmax],
  "schema": [
    {"name": "X", "type": "signed", "size": 4},
    {"name": "Y", "type": "signed", "size": 4},
    {"name": "Z", "type": "signed", "size": 4},
    {"name": "Intensity", "type": "unsigned", "size": 2},
    {"name": "ReturnNumber", "type": "unsigned", "size": 1},
    {"name": "Classification", "type": "unsigned", "size": 1},
    {"name": "GpsTime", "type": "float", "size": 8},
    {"name": "Red", "type": "unsigned", "size": 2},
    {"name": "Green", "type": "unsigned", "size": 2},
    {"name": "Blue", "type": "unsigned", "size": 2}
  ],
  "srs": {"wkt": "...", "authority": "EPSG", "horizontal": "2993"},
  "span": 256,
  "dataType": "laszip",
  "hierarchyType": "json",
  "version": "1.1.0"
}
```

### Hierarchy Files

`ept-hierarchy/0-0-0-0.json`:
```json
{
  "0-0-0-0": 65341,
  "1-0-0-0": 438,
  "1-0-0-1": -1,
  "1-1-0-0": 0
}
```

Values: positive = point count, 0 = empty node, -1 = sub-hierarchy at `ept-hierarchy/D-X-Y-Z.json`.

### Tile Files

Located at `ept-data/D-X-Y-Z.laz`. Standard LAZ files, each containing points for one octree node.

### Octree Geometry

The EPT octree divides a cube (from `bounds`) into 8 children at each level. For a key `(D, X, Y, Z)`:
- Cube size at depth D = `total_size / 2^D`
- Node origin: `(bounds_min + X * size, bounds_min + Y * size, bounds_min + Z * size)`

This is the same octree math already implemented in `EPTkey`/`EPToctree` in `src/vendor/LASzip/lascopc.hpp`.

## Reusable Existing Code

| Component | Location | Reuse |
|-----------|----------|-------|
| `EPTkey` struct | `src/vendor/LASzip/lascopc.hpp:56-69` | Octree key representation |
| `EPTKeyHasher` | `src/vendor/LASzip/lascopc.hpp:71-81` | HashMap key for nodes |
| `spatial_order()` | `src/vendor/LASzip/lascopc.hpp:115-137` | Tile ordering |
| `ByteStreamInGDAL` | `src/vendor/LASzip/bytestreamin_gdal.hpp` | Remote file I/O |
| `is_remote_path()` | `src/LASRcore/FileCollection.cpp:34-45` | URL detection |
| `filename_from_url()` | `src/LASRcore/FileCollection.cpp:47-60` | Name extraction |
| `nlohmann/json` | `src/vendor/nlohmann/json.hpp` | JSON parsing |
| `LASreaderLAS` | `src/vendor/LASlib/lasreader_las.cpp` | Tile LAZ reading |

## Schema Mapping

| EPT Schema Name | LAS Attribute | Determines Point Format |
|-----------------|---------------|------------------------|
| X, Y, Z | Coordinates | Always present |
| Intensity | Intensity | Always present |
| ReturnNumber | Return Number | Always present |
| NumberOfReturns | Number of Returns | Always present |
| Classification | Classification | Always present |
| ScanAngleRank | Scan Angle | Always present |
| UserData | User Data | Always present |
| PointSourceId | Point Source ID | Always present |
| GpsTime | GPS Time | Format 1/3/6/7/8 |
| Red, Green, Blue | RGB | Format 2/3/7/8 |
| Infrared | NIR | Format 8 |
| Other names | Extra Bytes | Added as extra bytes |

The point data format is inferred from which schema entries are present, using the existing `LASio::guess_point_data_format()` logic.

## Error Handling

| Condition | Behavior |
|-----------|----------|
| `dataType != "laszip"` | Error: "EPT dataset uses {type} format, only laszip is supported" |
| Invalid/missing ept.json | Error: "Failed to parse EPT metadata: {details}" |
| Missing hierarchy file | Error: "EPT hierarchy file not found: {path}" |
| Missing tile file | Warning + skip tile (partial datasets are common) |
| Network failure mid-read | Error propagated from GDAL VSI |
| No tiles in query region | Return 0 points (same as empty COPC query) |

## Verification Plan

### Test Data
- Create a small EPT dataset from the existing `inst/extdata/` test data using Entwine or PDAL
- Bundle in `inst/extdata/ept-test/` (ept.json + ept-hierarchy/ + ept-data/)

### Tests

1. **Local EPT read** — Read local EPT, verify point count matches source LAZ
2. **Remote EPT read** — Serve over httpuv, verify matches local read
3. **Spatial query** — `reader_circles()` on EPT, verify subset of points
4. **Depth control** — `ept_depth=0` returns root only, verify fewer points
5. **Error handling** — Non-laszip dataType, invalid URL, malformed JSON
6. **Pipeline integration** — `reader() + summarise()` on EPT, verify output
7. **Large dataset** — (skipped on CRAN) Read from public EPT endpoint

### Manual Verification
```r
# Local
ept <- system.file("extdata", "ept-test", "ept.json", package = "lasR")
ans <- exec(reader() + summarise(), on = ept)

# Remote
url <- "https://na-c.entwine.io/dk/ept.json"
ans <- exec(reader(ept_depth = 2) + summarise(), on = url)

# Spatial query
ans <- exec(reader_circles(x, y, r) + write_las(), on = url)
```

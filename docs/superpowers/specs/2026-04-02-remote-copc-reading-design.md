# Remote COPC/LAS Reading via GDAL Virtual Filesystems

**Date:** 2026-04-02
**Status:** Approved

## Problem

lasR currently requires all point cloud files to be on the local filesystem. COPC (Cloud Optimized Point Cloud) files are designed for efficient partial reads over HTTP via range requests, but this capability is unused. Users cannot process remote datasets without downloading them first.

## Goal

Enable lasR to read LAS/LAZ/COPC files over HTTP(S) and GDAL virtual filesystems (`/vsicurl/`, `/vsis3/`, `/vsiaz/`, `/vsigs/`), leveraging COPC's cloud-optimized octree index for efficient partial reads via HTTP range requests.

- **Phase 1:** HTTPS support via GDAL's `/vsicurl/`
- **Phase 2:** Cloud storage protocols (S3, Azure Blob, GCS) come free via GDAL `/vsi*` pass-through

## Design Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Where to implement | Inside vendored LASlib fork | LASlib is already heavily modified (COPC writer, CRAN patches). Adding a ByteStreamIn subclass is a natural extension. |
| HTTP library | GDAL's `/vsicurl/` | Already linked, `VSILFILE*` API maps 1:1 to `FILE*`, S3/Azure/GCS come free. No new dependency. |
| Non-COPC remote files | Allowed with size warning | Files >500MB without `.copc.` in the name trigger a warning recommending COPC. Not a hard error. |
| API surface | Transparent URL detection | `reader("https://...")` just works. `/vsi*` prefixes passed through for power users. No new functions or parameters. |
| Caching | GDAL built-in only | GDAL's `/vsicurl/` block cache is well-tested and configurable via env vars. No custom caching in Phase 1. |
| Testing | httpuv (R) + http.server (Python) for CRAN-safe tests, plus skip_on_cran integration tests against public endpoints | Reliable CI + real-world validation. |

## Architecture

### Component 1: `ByteStreamInGDAL` (new file)

**File:** `src/vendor/LASzip/bytestreamin_gdal.hpp`

A header-only file mirroring the existing `bytestreamin_file.hpp` structure. Three classes:

```
ByteStreamInGDAL (base)      -- holds VSILFILE*, implements getByte/getBytes/seek/tell/seekEnd
  ByteStreamInGDALLE         -- little-endian read methods
  ByteStreamInGDALBE         -- big-endian read methods
```

API mapping:

| ByteStreamInFile (FILE*) | ByteStreamInGDAL (VSILFILE*) |
|--------------------------|------------------------------|
| `fread()`                | `VSIFReadL()`                |
| `fseeko()`               | `VSIFSeekL()`                |
| `ftello()`               | `VSIFTellL()`                |
| `getc()`                 | `VSIFReadL(&byte, 1, 1, ...)` |
| N/A (caller closes)      | `VSIFCloseL()` in destructor |

Key difference from `ByteStreamInFile`: the GDAL variant **takes ownership** of the `VSILFILE*` and closes it in its destructor. This is because the handle is created solely for the stream and has no other consumer.

The entire file is guarded by `#ifdef USING_GDAL` so LASzip compiles standalone.

### Component 2: URL Detection and Routing in `LASreaderLAS::open()`

**File:** `src/vendor/LASlib/lasreader_las.cpp` (modify `open(const char* file_name, ...)`)

Flow:

```
open(file_name)
  |
  +-- starts with "http://", "https://", "/vsicurl/", "/vsis3/", "/vsigs/", "/vsiaz/"?
  |     |
  |     YES --> #ifdef USING_GDAL
  |     |         +-- Auto-prefix "http(s)://" with "/vsicurl/" (if no /vsi prefix)
  |     |         +-- VSIStatL() for non-COPC files: warn if >500MB
  |     |         +-- VSIFOpenL(path, "rb")
  |     |         +-- Create ByteStreamInGDALLE/BE
  |     |         +-- call open(ByteStreamIn*) -- existing overload, everything downstream unchanged
  |     |       #else
  |     |         +-- ERROR: "remote file access requires GDAL"
  |     |       #endif
  |     |
  |     NO ---> fopen() --> ByteStreamInFileLE/BE --> open(ByteStreamIn*)  [current path, untouched]
```

Implementation details:
- `FILE* file` member left as `nullptr` for the GDAL path. Existing `close()` already null-checks before `fclose()`.
- `setvbuf()` call skipped in the GDAL path (GDAL manages its own buffering).
- `file_name` member still set for error messages.

### Component 3: `FileCollection` Changes

**File:** `src/LASRcore/FileCollection.cpp` and `FileCollection.h`

Changes:

1. **New enum value:** `PathType::REMOTEFILE`

2. **New helper:** `is_remote_path(const std::string& path)` -- checks for `http://`, `https://`, `/vsicurl/`, `/vsis3/`, `/vsigs/`, `/vsiaz/` prefixes. Used in multiple places.

3. **`parse_path()`** -- Check for remote path before `std::filesystem::exists()`:
   ```
   if is_remote_path(path) -> return REMOTEFILE
   else -> existing filesystem logic
   ```

4. **`read()`** -- New branch for `REMOTEFILE` routing to `add_las_file()`. This works because `add_las_file()` calls `LASio::open()` which reaches `LASreaderLAS::open()` and hits the GDAL path. Header reading uses HTTP range requests (just the header + COPC index VLRs, not the entire file).

5. **`read_vpc()`** -- Skip `std::filesystem::weakly_canonical()` path resolution for remote URLs in VPC `href` fields:
   ```
   if is_remote_path(file_path) -> use as-is
   else -> resolve relative to parent folder
   ```

6. **Chunk naming** -- For remote files, extract the filename from the URL (last path segment, strip query parameters and extension) instead of using `std::filesystem::path::stem()`.

### Component 4: R/Python API

**No changes required.** The URL flows as a string through the existing path:

```
R/Python API --> C++ api.cpp --> FileCollection --> Chunk --> LASio --> LASreadOpener
  --> LASreaderLAS::open() --> URL detected --> GDAL path
```

Existing parameters work unchanged:
```r
reader("https://example.com/file.copc.laz")
reader("https://example.com/file.copc.laz", copc_depth = 2)
reader("/vsis3/my-bucket/scan.copc.laz")
```

### Component 5: Build System

**R package (`src/Makevars.in`):** No changes. `USING_GDAL` is already defined. `bytestreamin_gdal.hpp` is header-only and included conditionally. GDAL headers already on the include path.

**Python (`python/CMakeLists.txt`):** Two changes to the `laszip` static library target:
1. Add `USING_GDAL=1` to `target_compile_definitions`
2. Add `${GDAL_INCLUDE_DIRS}` to include directories

This is needed because `lasreader_las.cpp` (compiled into `laszip` via the `pylasr` target, but `bytestreamin_gdal.hpp` will be included from the LASzip layer) needs `cpl_vsi.h`.

Note: Looking at the CMake more carefully, `lasreader_las.cpp` is compiled as part of the `pylasr` target (line 273), not the `laszip` target. So the `laszip` static library may not need changes -- only `pylasr` needs `USING_GDAL`, which it already has (line 308). Verify during implementation.

### Component 6: Caching

Rely entirely on GDAL's built-in `/vsicurl/` block caching. Users can tune via environment variables:
- `CPL_VSIL_CURL_CACHE_SIZE` -- cache size in bytes (default ~16MB)
- `GDAL_HTTP_MAX_RETRY` -- retry count
- `GDAL_HTTP_RETRY_DELAY` -- retry delay in seconds
- `VSI_CACHE` -- enable/disable VSI caching
- `VSI_CACHE_SIZE` -- VSI cache size

No custom caching in lasR.

### Component 7: Testing

**R tests (`tests/testthat/test-remote.R`):**

CRAN-safe tests using `httpuv` to serve local files over HTTP:
```r
httpuv_server <- httpuv::startServer("127.0.0.1", port,
  list(staticPaths = list("/" = test_path("data"))))
withr::defer(httpuv::stopServer(httpuv_server))
```

Test cases:
- Remote COPC read matches local read (depth 0, 1, 2)
- Remote COPC with spatial query (circle, rectangle)
- Size warning for non-COPC remote file >500MB (mock or check message)
- Error on invalid URL
- `/vsi*` prefix pass-through works

`httpuv` added to `Suggests` in DESCRIPTION.

Integration tests (skip on CRAN):
```r
skip_on_cran()
skip_if_offline()
# Test against a well-known public COPC endpoint
```

**Python tests:** Mirror the same structure using `http.server` from Python stdlib.

## Out of Scope

- Writing to remote destinations (read-only in this design)
- Custom caching beyond GDAL's built-in
- Authentication UI (users configure via GDAL environment variables: `AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, `AZURE_STORAGE_ACCOUNT`, etc.)
- Support for remote PCD files
- Parallel HTTP connections for multi-file scenarios (each OpenMP thread gets its own GDAL handle naturally)

## Key Files to Modify

| File | Change |
|------|--------|
| `src/vendor/LASzip/bytestreamin_gdal.hpp` | **NEW** -- ByteStreamInGDAL classes |
| `src/vendor/LASlib/lasreader_las.cpp` | URL detection, GDAL open path |
| `src/LASRcore/FileCollection.h` | `REMOTEFILE` enum, `is_remote_path()` |
| `src/LASRcore/FileCollection.cpp` | `parse_path()`, `read()`, `read_vpc()`, chunk naming |
| `python/CMakeLists.txt` | Possibly add GDAL to laszip target |
| `DESCRIPTION` | Add `httpuv` to Suggests |
| `tests/testthat/test-remote.R` | **NEW** -- remote reading tests |

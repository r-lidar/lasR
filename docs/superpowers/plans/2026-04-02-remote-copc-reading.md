# Remote COPC/LAS Reading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable lasR to read LAS/LAZ/COPC files over HTTP(S) and GDAL virtual filesystems by creating a `ByteStreamInGDAL` class and routing URLs through GDAL's `/vsicurl/`.

**Architecture:** A new `ByteStreamInGDAL` header wraps GDAL's `VSILFILE*` to implement LASzip's `ByteStreamIn` interface. URL detection in `LASreaderLAS::open()` routes remote paths through this stream. `FileCollection` is updated to recognize URLs as valid file paths. All guarded by `#ifdef USING_GDAL`.

**Tech Stack:** C++17, GDAL (cpl_vsi.h), R (httpuv for tests), Python (http.server for tests)

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/vendor/LASzip/bytestreamin_gdal.hpp` | CREATE | ByteStreamInGDAL, ByteStreamInGDALLE, ByteStreamInGDALBE classes |
| `src/vendor/LASlib/lasreader_las.cpp` | MODIFY | URL detection, GDAL open path in `open(const char*)` |
| `src/LASRcore/FileCollection.h` | MODIFY | Add `REMOTEFILE` to `PathType` enum, declare `is_remote_path()` |
| `src/LASRcore/FileCollection.cpp` | MODIFY | `parse_path()`, `read()`, `read_vpc()`, chunk naming for URLs |
| `python/CMakeLists.txt` | MODIFY | Add GDAL to `laszip` target if needed |
| `DESCRIPTION` | MODIFY | Add `httpuv` to Suggests |
| `tests/testthat/test-remote.R` | CREATE | R tests for remote reading |

---

### Task 1: Create `ByteStreamInGDAL` Header

**Files:**
- Create: `src/vendor/LASzip/bytestreamin_gdal.hpp`

- [ ] **Step 1: Create the header file**

Create `src/vendor/LASzip/bytestreamin_gdal.hpp` with the full implementation. This mirrors `bytestreamin_file.hpp` exactly, replacing `FILE*` calls with GDAL `VSILFILE*` equivalents:

```cpp
/*
===============================================================================

  FILE:  bytestreamin_gdal.hpp

  CONTENTS:

    Class for GDAL VSILFILE*-based input streams with endian handling.
    Enables reading LAS/LAZ/COPC files over HTTP, S3, Azure, GCS via
    GDAL's virtual filesystem (/vsicurl/, /vsis3/, etc.).

  PROGRAMMERS:

    jean-romain.roussel@r-lidar.com

  COPYRIGHT:

    (c) 2026, Jean-Romain Roussel

    This is free software; you can redistribute and/or modify it under the
    terms of the GNU Lesser General Licence as published by the Free Software
    Foundation. See the COPYING file for more information.

    This software is distributed WITHOUT ANY WARRANTY and without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

===============================================================================
*/
#ifndef BYTE_STREAM_IN_GDAL_H
#define BYTE_STREAM_IN_GDAL_H

#ifdef USING_GDAL

#include "bytestreamin.hpp"
#include <cpl_vsi.h>

class ByteStreamInGDAL : public ByteStreamIn
{
public:
  ByteStreamInGDAL(VSILFILE* file);
  U32 getByte();
  void getBytes(U8* bytes, const U32 num_bytes);
  BOOL isSeekable() const;
  I64 tell() const;
  BOOL seek(const I64 position);
  BOOL seekEnd(const I64 distance=0);
  ~ByteStreamInGDAL();
protected:
  VSILFILE* file;
};

class ByteStreamInGDALLE : public ByteStreamInGDAL
{
public:
  ByteStreamInGDALLE(VSILFILE* file);
  void get16bitsLE(U8* bytes);
  void get32bitsLE(U8* bytes);
  void get64bitsLE(U8* bytes);
  void get16bitsBE(U8* bytes);
  void get32bitsBE(U8* bytes);
  void get64bitsBE(U8* bytes);
private:
  U8 swapped[8];
};

class ByteStreamInGDALBE : public ByteStreamInGDAL
{
public:
  ByteStreamInGDALBE(VSILFILE* file);
  void get16bitsLE(U8* bytes);
  void get32bitsLE(U8* bytes);
  void get64bitsLE(U8* bytes);
  void get16bitsBE(U8* bytes);
  void get32bitsBE(U8* bytes);
  void get64bitsBE(U8* bytes);
private:
  U8 swapped[8];
};

inline ByteStreamInGDAL::ByteStreamInGDAL(VSILFILE* file)
{
  this->file = file;
}

inline ByteStreamInGDAL::~ByteStreamInGDAL()
{
  if (file)
  {
    VSIFCloseL(file);
    file = 0;
  }
}

inline U32 ByteStreamInGDAL::getByte()
{
  U8 byte;
  if (VSIFReadL(&byte, 1, 1, file) != 1)
  {
    throw EOF;
  }
  return (U32)byte;
}

inline void ByteStreamInGDAL::getBytes(U8* bytes, const U32 num_bytes)
{
  if (VSIFReadL(bytes, 1, num_bytes, file) != num_bytes)
  {
    throw EOF;
  }
}

inline BOOL ByteStreamInGDAL::isSeekable() const
{
  return TRUE;
}

inline I64 ByteStreamInGDAL::tell() const
{
  return (I64)VSIFTellL(file);
}

inline BOOL ByteStreamInGDAL::seek(const I64 position)
{
  if (tell() != position)
  {
    return !(VSIFSeekL(file, (vsi_l_offset)position, SEEK_SET));
  }
  return TRUE;
}

inline BOOL ByteStreamInGDAL::seekEnd(const I64 distance)
{
  return !(VSIFSeekL(file, (vsi_l_offset)(-distance), SEEK_END));
}

inline ByteStreamInGDALLE::ByteStreamInGDALLE(VSILFILE* file) : ByteStreamInGDAL(file)
{
}

inline void ByteStreamInGDALLE::get16bitsLE(U8* bytes)
{
  getBytes(bytes, 2);
}

inline void ByteStreamInGDALLE::get32bitsLE(U8* bytes)
{
  getBytes(bytes, 4);
}

inline void ByteStreamInGDALLE::get64bitsLE(U8* bytes)
{
  getBytes(bytes, 8);
}

inline void ByteStreamInGDALLE::get16bitsBE(U8* bytes)
{
  getBytes(swapped, 2);
  bytes[0] = swapped[1];
  bytes[1] = swapped[0];
}

inline void ByteStreamInGDALLE::get32bitsBE(U8* bytes)
{
  getBytes(swapped, 4);
  bytes[0] = swapped[3];
  bytes[1] = swapped[2];
  bytes[2] = swapped[1];
  bytes[3] = swapped[0];
}

inline void ByteStreamInGDALLE::get64bitsBE(U8* bytes)
{
  getBytes(swapped, 8);
  bytes[0] = swapped[7];
  bytes[1] = swapped[6];
  bytes[2] = swapped[5];
  bytes[3] = swapped[4];
  bytes[4] = swapped[3];
  bytes[5] = swapped[2];
  bytes[6] = swapped[1];
  bytes[7] = swapped[0];
}

inline ByteStreamInGDALBE::ByteStreamInGDALBE(VSILFILE* file) : ByteStreamInGDAL(file)
{
}

inline void ByteStreamInGDALBE::get16bitsLE(U8* bytes)
{
  getBytes(swapped, 2);
  bytes[0] = swapped[1];
  bytes[1] = swapped[0];
}

inline void ByteStreamInGDALBE::get32bitsLE(U8* bytes)
{
  getBytes(swapped, 4);
  bytes[0] = swapped[3];
  bytes[1] = swapped[2];
  bytes[2] = swapped[1];
  bytes[3] = swapped[0];
}

inline void ByteStreamInGDALBE::get64bitsLE(U8* bytes)
{
  getBytes(swapped, 8);
  bytes[0] = swapped[7];
  bytes[1] = swapped[6];
  bytes[2] = swapped[5];
  bytes[3] = swapped[4];
  bytes[4] = swapped[3];
  bytes[5] = swapped[2];
  bytes[6] = swapped[1];
  bytes[7] = swapped[0];
}

inline void ByteStreamInGDALBE::get16bitsBE(U8* bytes)
{
  getBytes(bytes, 2);
}

inline void ByteStreamInGDALBE::get32bitsBE(U8* bytes)
{
  getBytes(bytes, 4);
}

inline void ByteStreamInGDALBE::get64bitsBE(U8* bytes)
{
  getBytes(bytes, 8);
}

#endif // USING_GDAL
#endif // BYTE_STREAM_IN_GDAL_H
```

- [ ] **Step 2: Verify the file compiles**

Run the R package build to check for compilation errors. The header is not yet included anywhere, but verify syntax:

```bash
cd /home/alex/projects/lasR && R CMD INSTALL --no-multiarch .
```

Expected: Successful install (header is not included yet, so no impact).

- [ ] **Step 3: Commit**

```bash
git add -f src/vendor/LASzip/bytestreamin_gdal.hpp
git commit -m "feat: add ByteStreamInGDAL class for VSILFILE*-based input streams"
```

---

### Task 2: Add URL Detection and GDAL Path in `LASreaderLAS::open()`

**Files:**
- Modify: `src/vendor/LASlib/lasreader_las.cpp:31-96`

- [ ] **Step 1: Add the GDAL header include**

At the top of `src/vendor/LASlib/lasreader_las.cpp`, after the existing includes (line 38), add the GDAL includes. The includes section (lines 31-46) should become:

```cpp
#include "lasreader_las.hpp"

#include "bytestreamin.hpp"
#include "bytestreamin_file.hpp"
#include "bytestreamin_istream.hpp"
#ifdef USING_GDAL
#include "bytestreamin_gdal.hpp"
#include <cpl_vsi.h>
#include <string>
#endif
#include "lasreadpoint.hpp"
#include "lasindex.hpp"
#include "lascopc.hpp"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include <stdlib.h>
#include <string.h>
```

- [ ] **Step 2: Add a static helper to detect remote paths**

Add this helper function before `LASreaderLAS::open()` (before line 48):

```cpp
static bool is_remote_path(const char* path)
{
  if (path == 0) return false;
  if (strncmp(path, "http://", 7) == 0) return true;
  if (strncmp(path, "https://", 8) == 0) return true;
  if (strncmp(path, "/vsicurl/", 9) == 0) return true;
  if (strncmp(path, "/vsis3/", 7) == 0) return true;
  if (strncmp(path, "/vsigs/", 7) == 0) return true;
  if (strncmp(path, "/vsiaz/", 7) == 0) return true;
  if (strncmp(path, "/vsiadls/", 9) == 0) return true;
  if (strncmp(path, "/vsioss/", 8) == 0) return true;
  if (strncmp(path, "/vsiswift/", 10) == 0) return true;
  return false;
}
```

- [ ] **Step 3: Modify `LASreaderLAS::open(const char*)` to handle URLs**

Replace the body of `LASreaderLAS::open(const char* file_name, I32 io_buffer_size, BOOL peek_only, U32 decompress_selective)` (lines 48-96) with:

```cpp
BOOL LASreaderLAS::open(const char* file_name, I32 io_buffer_size, BOOL peek_only, U32 decompress_selective)
{
  if (file_name == 0)
  {
    eprint("ERROR: file name pointer is zero\n");
    return FALSE;
  }

  if (is_remote_path(file_name))
  {
#ifdef USING_GDAL
    // Build the GDAL virtual filesystem path
    std::string vsi_path;
    if (strncmp(file_name, "http://", 7) == 0 || strncmp(file_name, "https://", 8) == 0)
    {
      vsi_path = std::string("/vsicurl/") + file_name;
    }
    else
    {
      vsi_path = file_name;
    }

    // Size warning for non-COPC files
    if (strstr(file_name, ".copc.") == NULL)
    {
      VSIStatBufL stat_buf;
      if (VSIStatL(vsi_path.c_str(), &stat_buf) == 0)
      {
        if (stat_buf.st_size > 500 * 1024 * 1024)
        {
          warning("WARNING: remote file '%s' is %.0f MB. Consider converting to COPC for efficient streaming.\n", file_name, (double)stat_buf.st_size / (1024.0 * 1024.0));
        }
      }
    }

    VSILFILE* vsi_file = VSIFOpenL(vsi_path.c_str(), "rb");
    if (vsi_file == 0)
    {
      eprint("ERROR: cannot open remote file '%s'\n", file_name);
      return FALSE;
    }

    // Save file name for error messages
    if (this->file_name)
    {
      free(this->file_name);
      this->file_name = 0;
    }
    this->file_name = LASCopyString(file_name);

    // Leave FILE* file as null -- close() already null-checks before fclose()
    // ByteStreamInGDAL takes ownership and will call VSIFCloseL in destructor

    ByteStreamIn* in;
    if (IS_LITTLE_ENDIAN())
      in = new ByteStreamInGDALLE(vsi_file);
    else
      in = new ByteStreamInGDALBE(vsi_file);

    return open(in, peek_only, decompress_selective);
#else
    eprint("ERROR: remote file access requires GDAL. File: '%s'\n", file_name);
    return FALSE;
#endif
  }

#ifdef _MSC_VER
  wchar_t* utf16_file_name = UTF8toUTF16(file_name);
  file = _wfopen(utf16_file_name, L"rb");
  if (file == 0)
  {
    eprint( "ERROR: cannot open file '%ws' for read\n", utf16_file_name);
  }
  delete [] utf16_file_name;
#else
  file = fopen(file_name, "rb");
#endif

  if (file == 0)
  {
    eprint( "ERROR: cannot open file '%s' for read\n", file_name);
    return FALSE;
  }

  // save file name for better ERROR message

  if (this->file_name)
  {
    free(this->file_name);
    this->file_name = 0;
  }
  this->file_name = LASCopyString(file_name);

  if (setvbuf(file, NULL, _IOFBF, io_buffer_size) != 0)
  {
    eprint( "WARNING: setvbuf() failed with buffer size %d\n", io_buffer_size);
  }

  // create input
  ByteStreamIn* in;
  if (IS_LITTLE_ENDIAN())
    in = new ByteStreamInFileLE(file);
  else
    in = new ByteStreamInFileBE(file);

  return open(in, peek_only, decompress_selective);
}
```

- [ ] **Step 4: Build and verify compilation**

```bash
cd /home/alex/projects/lasR && R CMD INSTALL --no-multiarch .
```

Expected: Successful compilation and install.

- [ ] **Step 5: Commit**

```bash
git add src/vendor/LASlib/lasreader_las.cpp
git commit -m "feat: route remote URLs through GDAL VSILFILE in LASreaderLAS::open()"
```

---

### Task 3: Update `FileCollection` to Handle Remote Paths

**Files:**
- Modify: `src/LASRcore/FileCollection.h:15`
- Modify: `src/LASRcore/FileCollection.cpp:36-110,140-264,685-810,890-919`

- [ ] **Step 1: Add `REMOTEFILE` to the `PathType` enum**

In `src/LASRcore/FileCollection.h`, line 15, change:

```cpp
enum PathType {DIRECTORY, VPCFILE, LASFILE, LAXFILE, PCDFILE, OTHERFILE, MISSINGFILE, UNKNOWNFILE, DATAFRAME, XPTR};
```

to:

```cpp
enum PathType {DIRECTORY, VPCFILE, LASFILE, LAXFILE, PCDFILE, OTHERFILE, MISSINGFILE, UNKNOWNFILE, DATAFRAME, XPTR, REMOTEFILE};
```

- [ ] **Step 2: Add `is_remote_path()` helper in `FileCollection.cpp`**

Add this function near the top of `src/LASRcore/FileCollection.cpp`, after the existing includes (after line 33):

```cpp
static bool is_remote_path(const std::string& path)
{
  if (path.compare(0, 7, "http://") == 0) return true;
  if (path.compare(0, 8, "https://") == 0) return true;
  if (path.compare(0, 9, "/vsicurl/") == 0) return true;
  if (path.compare(0, 7, "/vsis3/") == 0) return true;
  if (path.compare(0, 7, "/vsigs/") == 0) return true;
  if (path.compare(0, 7, "/vsiaz/") == 0) return true;
  if (path.compare(0, 9, "/vsiadls/") == 0) return true;
  if (path.compare(0, 8, "/vsioss/") == 0) return true;
  if (path.compare(0, 10, "/vsiswift/") == 0) return true;
  return false;
}
```

- [ ] **Step 3: Add a helper to extract filename from URL**

Add this function right after `is_remote_path()`:

```cpp
static std::string filename_from_url(const std::string& url)
{
  // Strip query parameters
  std::string path = url.substr(0, url.find('?'));
  // Find the last slash
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) return path;
  std::string name = path.substr(pos + 1);
  // Strip .laz or .las extension, and .copc prefix if present
  size_t dot = name.rfind('.');
  if (dot != std::string::npos) name = name.substr(0, dot);
  if (name.size() > 5 && name.substr(name.size() - 5) == ".copc")
    name = name.substr(0, name.size() - 5);
  return name;
}
```

- [ ] **Step 4: Update `parse_path()` to detect remote paths**

In `src/LASRcore/FileCollection.cpp`, modify the `parse_path()` method (lines 890-919). Add the remote check at the start of the function, before `std::filesystem::exists()`:

```cpp
PathType FileCollection::parse_path(const std::string& path)
{
  if (is_remote_path(path))
  {
    return PathType::REMOTEFILE;
  }

  std::filesystem::path file_path(path);

  if (std::filesystem::exists(file_path))
  {
    if (std::filesystem::is_regular_file(file_path))
    {
      std::string ext = file_path.extension().string();
      if (ext == ".vpc" || ext == ".vpc") return PathType::VPCFILE;
      if (ext == ".las" || ext == ".LAS") return PathType::LASFILE;
      if (ext == ".laz" || ext == ".LAZ") return PathType::LASFILE;
      if (ext == ".lax" || ext == ".LAX") return PathType::LAXFILE;
      if (ext == ".pcd" || ext == ".PCD") return PathType::PCDFILE;
      return PathType::OTHERFILE;
    }
    else if (std::filesystem::is_directory(path))
    {
      return PathType::DIRECTORY;
    }
    else
    {
      return PathType::UNKNOWNFILE;
    }
  }
  else
  {
    return PathType::MISSINGFILE;
  }
}
```

- [ ] **Step 5: Add `REMOTEFILE` branch in `read()`**

In `src/LASRcore/FileCollection.cpp`, in the `read()` method, add a new `else if` branch after the `LASFILE` case (after line 60):

```cpp
    else if (type == PathType::REMOTEFILE)
    {
      if (!add_las_file(file)) return false;
    }
```

- [ ] **Step 6: Update `read_vpc()` to handle remote URLs in href**

In `src/LASRcore/FileCollection.cpp`, in the `read_vpc()` method, modify the path resolution at lines 197-198. Change:

```cpp
      std::string file_path = first_asset["href"];
      file_path = std::filesystem::weakly_canonical(parent_folder / file_path).string();
```

to:

```cpp
      std::string file_path = first_asset["href"];
      if (!is_remote_path(file_path))
      {
        file_path = std::filesystem::weakly_canonical(parent_folder / file_path).string();
      }
```

- [ ] **Step 7: Update chunk naming for remote files**

In `src/LASRcore/FileCollection.cpp`, in `get_chunk_regular()`, change lines 696-700 from:

```cpp
  if (!use_dataframe)
  {
    chunk.main_files.push_back(files[i].string());
    chunk.name = files[i].stem().string();
  }
```

to:

```cpp
  if (!use_dataframe)
  {
    std::string filepath = files[i].string();
    chunk.main_files.push_back(filepath);
    if (is_remote_path(filepath))
      chunk.name = filename_from_url(filepath);
    else
      chunk.name = files[i].stem().string();
  }
```

- [ ] **Step 8: Build and verify compilation**

```bash
cd /home/alex/projects/lasR && R CMD INSTALL --no-multiarch .
```

Expected: Successful compilation and install.

- [ ] **Step 9: Commit**

```bash
git add src/LASRcore/FileCollection.h src/LASRcore/FileCollection.cpp
git commit -m "feat: support remote file paths in FileCollection"
```

---

### Task 4: Update Python Build System

**Files:**
- Modify: `python/CMakeLists.txt:143-175`

- [ ] **Step 1: Check if `laszip` target needs GDAL**

The `laszip` static library in `python/CMakeLists.txt` compiles LASzip source files (lines 143-164). `lasreader_las.cpp` is NOT in this target -- it is in the `pylasr` target (line 273). Since `pylasr` already has `USING_GDAL=1` (line 308) and GDAL includes (line 139), no changes to the `laszip` target should be needed.

Verify by building:

```bash
cd /home/alex/projects/lasR/python && pip install -e .
```

Expected: Successful build. If it fails because `laszip` target code somehow includes the new header, add `USING_GDAL=1` and GDAL includes to the `laszip` target. In `python/CMakeLists.txt`, after line 175, add:

```cmake
if(GDAL_FOUND)
  target_compile_definitions(laszip PRIVATE USING_GDAL=1)
  target_include_directories(laszip PRIVATE ${GDAL_INCLUDE_DIRS})
endif()
```

- [ ] **Step 2: Commit if changes were needed**

```bash
git add python/CMakeLists.txt
git commit -m "build: ensure GDAL available for ByteStreamInGDAL in Python build"
```

---

### Task 5: Write R Tests for Remote Reading

**Files:**
- Modify: `DESCRIPTION:29-34`
- Create: `tests/testthat/test-remote.R`

- [ ] **Step 1: Add `httpuv` to Suggests in DESCRIPTION**

In `DESCRIPTION`, modify the `Suggests:` section (lines 29-34). Change:

```
Suggests: 
    knitr,
    rmarkdown,
    sf,
    terra,
    testthat (>= 3.0.0),
```

to:

```
Suggests: 
    httpuv,
    knitr,
    rmarkdown,
    sf,
    terra,
    testthat (>= 3.0.0),
```

- [ ] **Step 2: Create the test file with local HTTP server tests**

Create `tests/testthat/test-remote.R`:

```r
test_that("remote COPC reads correctly via HTTP",
{
  skip_if_not_installed("httpuv")

  f <- system.file("extdata", "example.copc.laz", package = "lasR")
  data_dir <- dirname(f)

  # Start a local HTTP server
  port <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port,
    list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/example.copc.laz")

  # Read remote
  pipeline_remote <- reader() + summarise()
  ans_remote <- exec(pipeline_remote, on = url)

  # Read local
  pipeline_local <- reader() + summarise()
  ans_local <- exec(pipeline_local, on = f)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote COPC with copc_depth works",
{
  skip_if_not_installed("httpuv")

  f <- system.file("extdata", "example.copc.laz", package = "lasR")
  data_dir <- dirname(f)

  port <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port,
    list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/example.copc.laz")

  pipeline_remote <- reader(copc_depth = 0) + summarise()
  ans_remote <- exec(pipeline_remote, on = url)

  pipeline_local <- reader(copc_depth = 0) + summarise()
  ans_local <- exec(pipeline_local, on = f)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote non-COPC file reads correctly",
{
  skip_if_not_installed("httpuv")

  f <- system.file("extdata", "Example.laz", package = "lasR")
  data_dir <- dirname(f)

  port <- httpuv::randomPort()
  server <- httpuv::startServer("127.0.0.1", port,
    list(staticPaths = list("/" = data_dir)))
  on.exit(httpuv::stopServer(server), add = TRUE)

  url <- paste0("http://127.0.0.1:", port, "/Example.laz")

  pipeline_remote <- reader() + summarise()
  ans_remote <- exec(pipeline_remote, on = url)

  pipeline_local <- reader() + summarise()
  ans_local <- exec(pipeline_local, on = f)

  expect_equal(ans_remote$npoints, ans_local$npoints)
})

test_that("remote file with invalid URL fails gracefully",
{
  expect_error(exec(reader() + summarise(), on = "https://localhost:99999/nonexistent.copc.laz"))
})

test_that("public remote COPC endpoint works",
{
  skip_on_cran()
  skip_if_offline()

  url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"

  pipeline <- reader(copc_depth = 0) + summarise()
  ans <- exec(pipeline, on = url)

  expect_true(ans$npoints > 0)
})
```

- [ ] **Step 3: Run the local HTTP server tests**

```bash
cd /home/alex/projects/lasR && Rscript -e 'testthat::test_file("tests/testthat/test-remote.R")'
```

Expected: All local tests pass. The public endpoint test may be skipped if offline.

- [ ] **Step 4: Run the full test suite to check for regressions**

```bash
cd /home/alex/projects/lasR && Rscript -e 'devtools::test()'
```

Expected: No new test failures.

- [ ] **Step 5: Commit**

```bash
git add DESCRIPTION tests/testthat/test-remote.R
git commit -m "test: add tests for remote COPC/LAS reading via HTTP"
```

---

### Task 6: Write Python Tests for Remote Reading

**Files:**
- Create: `python/tests/test_remote.py`

- [ ] **Step 1: Create the Python test file**

Create `python/tests/test_remote.py`:

```python
import pytest
import threading
import http.server
import os
import sys

# Add parent directory to path for pylasr import
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))


def get_test_data_dir():
    """Find the inst/extdata directory with test COPC files."""
    base = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    return os.path.join(base, "inst", "extdata")


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # suppress request logging during tests


@pytest.fixture(scope="module")
def http_server():
    """Start a local HTTP server serving the test data directory."""
    data_dir = get_test_data_dir()
    original_dir = os.getcwd()
    os.chdir(data_dir)
    server = http.server.HTTPServer(("127.0.0.1", 0), QuietHandler)
    port = server.server_address[1]
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    yield f"http://127.0.0.1:{port}"
    server.shutdown()
    os.chdir(original_dir)


def test_remote_copc_read(http_server):
    """Test that remote COPC reading matches local reading."""
    import pylasr

    local_file = os.path.join(get_test_data_dir(), "example.copc.laz")
    remote_url = f"{http_server}/example.copc.laz"

    # Read local
    pipeline_local = pylasr.reader_coverage() + pylasr.summarise()
    ans_local = pylasr.exec(pipeline_local, on=local_file)

    # Read remote
    pipeline_remote = pylasr.reader_coverage() + pylasr.summarise()
    ans_remote = pylasr.exec(pipeline_remote, on=remote_url)

    assert ans_remote["npoints"] == ans_local["npoints"]


def test_remote_copc_depth(http_server):
    """Test that copc_depth works with remote files."""
    import pylasr

    local_file = os.path.join(get_test_data_dir(), "example.copc.laz")
    remote_url = f"{http_server}/example.copc.laz"

    pipeline_local = pylasr.reader_coverage(copc_depth=0) + pylasr.summarise()
    ans_local = pylasr.exec(pipeline_local, on=local_file)

    pipeline_remote = pylasr.reader_coverage(copc_depth=0) + pylasr.summarise()
    ans_remote = pylasr.exec(pipeline_remote, on=remote_url)

    assert ans_remote["npoints"] == ans_local["npoints"]


@pytest.mark.skipif(
    os.environ.get("CI") == "true" or os.environ.get("CRAN") == "true",
    reason="Skip public endpoint tests in CI/CRAN"
)
def test_public_copc_endpoint():
    """Test against a real public COPC endpoint."""
    import pylasr

    url = "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"
    pipeline = pylasr.reader_coverage(copc_depth=0) + pylasr.summarise()
    ans = pylasr.exec(pipeline, on=url)
    assert ans["npoints"] > 0
```

- [ ] **Step 2: Run the Python tests**

```bash
cd /home/alex/projects/lasR/python && python -m pytest tests/test_remote.py -v
```

Expected: Local HTTP server tests pass.

- [ ] **Step 3: Commit**

```bash
git add python/tests/test_remote.py
git commit -m "test: add Python tests for remote COPC/LAS reading"
```

---

### Task 7: Integration Verification

- [ ] **Step 1: Run the full R test suite**

```bash
cd /home/alex/projects/lasR && Rscript -e 'devtools::test()'
```

Expected: All existing tests pass, plus the new remote tests.

- [ ] **Step 2: Run the full Python test suite**

```bash
cd /home/alex/projects/lasR/python && python -m pytest tests/ -v
```

Expected: All tests pass.

- [ ] **Step 3: Manual smoke test with a real remote COPC file**

```r
library(lasR)
url <- "https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"
pipeline <- reader(copc_depth = 0) + summarise()
ans <- exec(pipeline, on = url)
print(ans)
```

Expected: Prints point count and summary statistics without downloading the full file.

- [ ] **Step 4: Verify /vsi* pass-through works**

```r
library(lasR)
url <- "/vsicurl/https://s3.amazonaws.com/hobu-lidar/autzen-classified.copc.laz"
pipeline <- reader(copc_depth = 0) + summarise()
ans <- exec(pipeline, on = url)
print(ans)
```

Expected: Same result as Step 3 -- the `/vsicurl/` prefix is passed through without double-prefixing.

- [ ] **Step 5: Final commit if any fixes were needed**

```bash
git add -A
git commit -m "fix: address integration test issues for remote reading"
```

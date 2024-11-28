# Vendor Directory

This directory contains third-party open source libraries and tools that are essential for the functioning of the `lasR` package.

## Subdirectories Overview

1. **Eigen**: Linear Algebra. https://eigen.tuxfamily.org/index.php?title=Main_Page
2. **LASlib/LASzip**: Read and write LAS/LAZ files. https://github.com/LAStools/LAStools
3. **csf**: Cloth Simulation Filter. https://github.com/jianboqi/CSF
4. **delaunator**: Delaunay triangulation. https://github.com/delfrrr/delaunator-cpp
5. **geophoton**: chm enhamcement. https://github.com/Geophoton-inc/chm_prep

## Notes
- These libraries are external and are maintained independently of the `lasR` package.
- Updates to these dependencies should be handled carefully to ensure stability and compatibility with the package.

## Licenses
Please refer to the individual LICENSE files in each subdirectory for terms and conditions related to the third-party software.

## LASlib and LASzip LGPL License and Modifications

### Modifications:
- Removed every call to `exit()` and `rand()` to comply with CRAN policies.
- Replaced all instances of `sprintf` with `snprintf` to comply with CRAN policies.
- Used `print()` to conditionally call `printf()` or `Rprintf()` to comply with CRAN policies (see L39 in `mydef.hpp`).
- Replaced some occurrences of `strncpy` with `memcpy` to bypass false-positive warnings from the CRAN compiler regarding non-null-terminated strings.
- Fixed various memory leaks (specific locations not remembered).
- Changed `stdout` usage to `return true` in `bytestreamount_file.hpp` (see L134) to comply with CRAN policies.
- Added extra filters in `lasfilter.cpp` (see LL617–1807) and extra flags (see L1959–2161).

All modifications are provided under the LGPL license.

### How to Compile Without LASlib/LASzip:

According to the LGPL license, users must be able to replace LASlib/LASzip. Since the libraries are statically linked (the only method permitted in an R package), users cannot simply swap out the `.so` or `.dll` files. To use an alternative library, users can modify a single file that serves as an interface: `LASRcore/LASlibInterface.cpp`. This approach complies with LGPL requirements.

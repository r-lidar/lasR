# lasR - Fast and Pipeable Airborne LiDAR Data Tools

## Project Overview
lasR is an R package designed for efficient airborne LiDAR data processing. It provides a platform to share efficient implementations of tools designed with the lidR package, enabling the creation and execution of complex processing pipelines on massive lidar data.

## Key Features
- Read/write `.las`, `.laz` and `.pcd` files
- Compute metrics using area-based approach
- Generate digital canopy models
- Segment individual trees
- Process collections of files using multicore processing
- Highly optimized C++ implementation with minimal R code
- Pipeline-based processing approach

## Architecture
- **Core**: C++ implementation in `src/LASRcore/`
- **Stages**: Processing stages in `src/LASRstages/`
- **Readers**: File I/O in `src/LASRreaders/`
- **API**: R bindings in `src/LASRapi/`
- **Python**: Python bindings in `python/`

## Build System
- Uses GNU Make with `Makefile`
- R package build via `configure` script
- Python build via `setup.py`
- CMake for Python extension

## Testing
- R tests in `tests/testthat/`
- Run with: `R CMD check` or `devtools::test()`

## Dependencies
- **Required**: GDAL (>= 2.2.3), GEOS (>= 3.4.0), PROJ (>= 4.9.3), sqlite3
- **Optional**: sf, terra (for enhanced user experience)
- **Build**: C++17, GNU make, Rcpp

## Development Notes
- Version 0.17.0 (current)
- License: GPL-3
- Main author: Jean-Romain Roussel
- Includes third-party libraries: LASlib, LASzip, chm_prep, json, delaunator, Eigen, CSF

## Common Commands
```bash
# Build R package
R CMD build .
R CMD check lasR_*.tar.gz

# Build Python extension
cd python
python setup.py build

# Run tests
R -e "devtools::test()"
```

## File Structure
- `R/`: R interface code
- `src/`: C++ source code
- `python/`: Python bindings
- `tests/`: Test suites
- `man/`: Documentation
- `inst/`: Installation files and example data
- `vignettes/`: Package vignettes
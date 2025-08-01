# pylasr Examples

This directory contains example scripts demonstrating various features of the pylasr Python API.

## Quick Start

All examples can be run in two modes:

1. **Demo Mode** (no data processing): Shows API features without requiring data files
2. **Processing Mode**: Processes your LiDAR data files

## Example Scripts

### `basic_usage.py`
Simple examples covering the most common use cases.

**Usage:**
```bash
# Demo mode - shows API features without data processing
python basic_usage.py

# Processing mode - processes your LiDAR data
python basic_usage.py your_file.las
```

**Features:**
- System information
- Basic pipeline creation
- Configuration options
- Data processing (when file provided)

### `complete_example.py`
Comprehensive demonstration of all major features.

**Usage:**
```bash
# Demo mode - shows advanced features without data processing
python complete_example.py

# Processing mode - processes your data and shows multithreading
python complete_example.py your_file.las
```

## Getting Example Data

### Option 1: Use the included example data
If you're running from the lasR repository:
```bash
python basic_usage.py ../inst/extdata/Example.las
```

### Option 2: Use your own LiDAR data
Any `.las` or `.laz` file will work:
```bash
python basic_usage.py /path/to/your/file.las
```

### Option 3: Download sample data
You can find sample LiDAR data from various sources:
- [OpenTopography](https://opentopography.org/)
- [USGS 3DEP](https://www.usgs.gov/3d-elevation-program)
- [State/local government GIS portals](https://www.fgdc.gov/dataandservices)


## Need Help?

- Check the main [README.md](../README.md) for full API documentation
- Look at the [lasR documentation](https://r-lidar.github.io/lasR/) for algorithm details
- File issues at the [lasR repository](https://github.com/r-lidar/lasR/issues) 
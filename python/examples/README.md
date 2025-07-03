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
- JSON export
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

**Features:**
- Advanced pipeline workflows
- Processing configuration
- Convenience functions
- Manual stage creation
- Multithreading comparison (when file provided)
- Error handling

### `create_pipelines.py`
Creates various pipeline JSON files for different workflows.

**Usage:**
```bash
python create_pipelines.py
```

**Creates:**
- `info_pipeline.json`
- `cleaning_pipeline.json`
- `terrain_pipeline.json`
- `sampling_pipeline.json`
- `classification_pipeline.json`

### `lasr_cli.py`
Enhanced command-line tool for pipeline inspection and processing.

**Usage:**
```bash
# Inspect pipeline
python lasr_cli.py info_pipeline.json

# Process files
python lasr_cli.py info_pipeline.json your_data.las
python lasr_cli.py terrain_pipeline.json file1.las file2.las
```

**Features:**
- Display pipeline information
- Execute pipelines with input files
- Show processing timing and results
- Support for multithreading configuration

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

## Key Improvements

**User-Friendly Data Handling:**
- No more hardcoded path probing
- Clear command-line interface
- Helpful error messages
- Example data location suggestions

**Better Learning Experience:**
- Demo mode for exploring features without data
- Processing mode for hands-on experience
- Multithreading demonstrations
- Performance comparisons

**Robust Error Handling:**
- File existence validation
- Clear usage instructions
- Graceful degradation when data unavailable

## Tips

1. **Start with demo mode** to understand the API without needing data files
2. **Use small files first** when testing processing features
3. **Check multithreading** with the complete example to optimize performance
4. **Create pipelines** with the create_pipelines script, then test with the CLI tool
5. **Explore different file formats** - pylasr supports .las, .laz, and .copc files

## Need Help?

- Check the main [README.md](../README.md) for full API documentation
- Look at the [lasR documentation](https://r-lidar.github.io/lasR/) for algorithm details
- File issues at the [lasR repository](https://github.com/r-lidar/lasR/issues) 
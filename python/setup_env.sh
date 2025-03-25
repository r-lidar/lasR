#!/bin/bash
# Improved script to set up the environment for building LASR Python bindings

# Create virtual environment directory if it doesn't exist
VENV_DIR="venv"
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment in $VENV_DIR..."
    python -m venv "$VENV_DIR"
    if [ $? -ne 0 ]; then
        echo "Failed to create virtual environment. Make sure python3-venv is installed."
        echo "On Ubuntu/Debian: sudo apt-get install python3-venv"
        exit 1
    fi
fi

# Activate virtual environment
source "$VENV_DIR/bin/activate"
echo "Virtual environment activated. Using Python: $(which python)"
echo "Python version: $(python --version)"

# Check if cmake is installed
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not found. Please install CMake 3.18 or higher."
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "On macOS: brew install cmake"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "On Ubuntu/Debian: sudo apt-get install cmake"
    fi
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | cut -d" " -f3)
echo "Found CMake version: $CMAKE_VERSION"

# Detect OS and install dependencies
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Detected macOS"
    
    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        echo "Homebrew not found. Please install it from https://brew.sh/"
        exit 1
    fi
    
    # Install GDAL
    echo "Installing GDAL via Homebrew..."
    brew install gdal
    
    # Get GDAL installation path
    GDAL_DIR=$(brew --prefix gdal)
    echo "GDAL installed at: $GDAL_DIR"
    export GDAL_DIR
    
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Detected Linux"
    
    # Try to install with the available package manager
    if command -v apt-get &> /dev/null; then
        echo "Installing GDAL via apt..."
        sudo apt-get update
        sudo apt-get install -y libgdal-dev
    elif command -v yum &> /dev/null; then
        echo "Installing GDAL via yum..."
        sudo yum install -y gdal-devel
    else
        echo "Unsupported Linux distribution. Please install GDAL manually."
        exit 1
    fi
    
    # Set GDAL environment variables if gdal-config is available
    if command -v gdal-config &> /dev/null; then
        GDAL_INCLUDE_DIR=$(gdal-config --cflags | cut -d ' ' -f 1 | sed 's/-I//')
        echo "GDAL include directory: $GDAL_INCLUDE_DIR"
        export GDAL_INCLUDE_DIR
    fi
else
    echo "Unsupported operating system: $OSTYPE"
    echo "Please install GDAL manually and set GDAL_DIR environment variable."
    exit 1
fi

# Quick check for C++17 compatible compiler
CXX=${CXX:-"$(command -v clang++ || command -v g++)"}
if [ -z "$CXX" ]; then
    echo "No C++ compiler found. Please install clang++ or g++."
    exit 1
fi

echo "Using C++ compiler: $CXX"
if ! $CXX -std=c++17 -x c++ - -o /dev/null 2>/dev/null <<< "#include <filesystem>"; then
    echo "ERROR: Your compiler doesn't support C++17 features."
    echo "Please install a modern C++ compiler (GCC 7+, Clang 5+, or MSVC 2017+)"
    exit 1
fi

# Install Python build dependencies in the virtual environment
echo "Installing Python build dependencies..."
pip install --upgrade pip wheel setuptools cmake pybind11

# Print pybind11 configuration
echo "Checking pybind11 installation..."
python -c "import pybind11; print(f'pybind11 version: {pybind11.__version__}')"

# Clean build directory if exists
if [ -d "build" ]; then
    echo "Cleaning previous build directory..."
    rm -rf build
fi

echo ""
echo "Environment setup complete. You can now build and install the Python bindings with:"
echo "pip install -e ."
echo ""
echo "To activate this environment in the future, run:"
echo "source $VENV_DIR/bin/activate"
echo "" 
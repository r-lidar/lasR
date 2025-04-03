#!/bin/bash
# Simplified script to install the Python module and run the example

# Get the directory of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VENV_DIR="$SCRIPT_DIR/venv"

# Check if virtual environment exists
if [ ! -d "$VENV_DIR" ]; then
    echo "Virtual environment not found. Please run setup_env.sh first."
    echo "./setup_env.sh"
    exit 1
fi

# Activate virtual environment
source "$VENV_DIR/bin/activate"
echo "Virtual environment activated. Using Python: $(which python)"

echo "Building and installing pylasr module..."

# Change to the python directory
cd "$SCRIPT_DIR"
 
# Build and install the module in development mode with OpenMP enabled
# This creates a link to the source directory, so changes are immediately reflected
export CMAKE_ARGS="-DUSE_OPENMP=ON"
pip install -e .

# Show information about build artifacts
echo -e "\n\033[1mBuild artifacts:\033[0m"
find . -name "*.so" -o -name "*.dylib" | grep -v "venv" 2>/dev/null || echo "No .so or .dylib files found in project directory"

echo -e "\n\033[1mContents of build directory:\033[0m"
if [ -d "build" ]; then
    find build -name "*.so" -o -name "*.dylib" -type f | sort
else
    echo "Build directory not found"
fi


# Check if the module was installed successfully
echo -e "\n\033[1mTesting module import:\033[0m"
IMPORT_TEST=$(python -c "import pylasr; print('Import successful')" 2>&1)
IMPORT_RESULT=$?

if [ $IMPORT_RESULT -ne 0 ]; then
    echo "Error: Failed to import pylasr module. Error details:"
    echo "$IMPORT_TEST"
    
    # Test import again
    echo -e "\n\033[1mTesting module import again:\033[0m"
    IMPORT_TEST=$(python -c "import pylasr; print('Import successful')" 2>&1)
    IMPORT_RESULT=$?
    
    if [ $IMPORT_RESULT -ne 0 ]; then
        echo "Error: Module installation fix failed. Error details:"
        echo "$IMPORT_TEST"
        exit 1
    fi
fi

echo "Module installed and imported successfully."

# Check if a config file was provided to run the example
if [ "$#" -eq 0 ]; then
    echo "Module is installed. To run the example, use:"
    echo "$0 <config_file.json>"
    exit 0
fi

# Run the example with the provided arguments
echo "Running example with config: $1"
python "$SCRIPT_DIR/example.py" "$@"

# Remind user about deactivation
echo ""
echo "To deactivate the virtual environment when finished, run: deactivate" 
#!/bin/bash
# Cleanup script to remove redundant directories and files

echo "Cleaning up redundant directories and preparing for a fresh build..."

# Remove redundant virtual environment directories
if [ -d ".venv" ]; then
    echo "Removing .venv directory..."
    rm -rf .venv
fi

# Remove build artifacts
echo "Removing build directories and artifacts..."
rm -rf build/
rm -rf *.egg-info/
rm -rf *.so
rm -rf __pycache__/
rm -rf .pytest_cache/

# Remove CMake cache files if they exist
find . -name CMakeCache.txt -delete
find . -name CMakeFiles -type d -exec rm -rf {} +

# Clean up pybind11 directory if it's a git clone
if [ -d "pybind11/.git" ]; then
    echo "Removing pybind11 directory (use pip install pybind11 instead)..."
    rm -rf pybind11
fi

echo "Cleanup complete. Now run setup_env.sh to create a fresh environment."
echo "./setup_env.sh" 